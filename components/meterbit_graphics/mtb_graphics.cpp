#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_sleep.h"
#include "sdkconfig.h"
#include "driver/rtc_io.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

#include "Arduino.h"
#include <ArduinoJson.h>
#include "mtb_fonts.h"
#include "mtb_graphics.h"
#include "mtb_system.h"
#include "mtb_text_scroll.h"
#include "lodepng.h"
#include "mtb_ntp.h"
#include "gifdec.h"
#include "mtb_engine.h"
#include "LittleFS.h"
#include "mtb_github.h"
#include <HTTPClient.h>
#include <esp_heap_caps.h>  // For PSRAM allocation on ESP32
#include "nanosvg.h"
#include "nanosvgrast.h"
#include "mtb_countries.h"
#include <cmath>

static const char *TAG = "Mtb_Graphics";

// SemaphoreHandle_t pngImageDrawer_Sem = NULL;
//  Define the static queue storage variable
EXT_RAM_BSS_ATTR StaticQueue_t xQueueStorage_PNG_LocalDrawings;
EXT_RAM_BSS_ATTR StaticQueue_t xQueueStorage_SVG_LocalDrawings;

EXT_RAM_BSS_ATTR QueueHandle_t pngLocalImageDrawer_Q = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t svgLocalImageDrawer_Q = NULL;

EXT_RAM_BSS_ATTR QueueHandle_t rgb_led_queue = NULL;
// QueueHandle_t statusBar_Q_Handle = NULL;
// SemaphoreHandle_t statusBar_Mutex_Handle = NULL;
// TaskHandle_t statusBarUpdateTask_h = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t pngLocalImageDrawer_Handle = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t svgLocalImageDrawer_Handle = NULL;

EXT_RAM_BSS_ATTR Mtb_LocalImage_t statusBarItems[11];
uint16_t currentStatusLEDcolor = BLACK;

    // Create and start driver
Hub75Driver* mtb_Display_Driver;
uint16_t panelBrightness = 70;
uint8_t **Mtb_FixedText_t::scratchPad = nullptr;
EXT_RAM_BSS_ATTR uint16_t mtb_Panel_Frame_Buffer[PANEL_RES_X][PANEL_RES_Y];

// Prefer internal DRAM for these local-drawer tasks to avoid PSRAM-as-stack instability.
EXT_RAM_BSS_ATTR Mtb_Services *mtb_Png_Local_ImageDrawer_Sv = new Mtb_Services(mtb_Draw_Local_Png_Task, &pngLocalImageDrawer_Handle, "PNG LOCAL DRAWER", 6144, 2); // Keep the task stack size at 12288 for reliability.
EXT_RAM_BSS_ATTR Mtb_Services *mtb_SvgLocal_ImageDrawer_Sv = new Mtb_Services(mtb_Draw_Local_Svg_Task, &svgLocalImageDrawer_Handle, "SVG LOCAL DRAWER", 6144, 2); // Keep the task stack size at 10240 for reliability.

#define RGB_LED_PIN_R 3
#define RGB_LED_PIN_G 42
#define RGB_LED_PIN_B 38

#define MAX_PNG_IMAGES 6
EXT_RAM_BSS_ATTR Mtb_PreloadedImage_t preloadedPNGs[MAX_PNG_IMAGES];

#define MAX_SVG_IMAGES 6
EXT_RAM_BSS_ATTR Mtb_PreloadedImage_t preloadedSVGs[MAX_SVG_IMAGES];

volatile int preloadIndex = 0;
volatile int downloadedPNGs = 0;
volatile int downloadedSVGs = 0;
SemaphoreHandle_t preloadIndexMutex = xSemaphoreCreateMutex();

void mtb_Do_Nothing_Void_Fn(void){}

// Simple error codes (extend as you like)
enum {
  PSRAM_COPY_OK = 0,
  PSRAM_COPY_ERR_BAD_ARG = 1,
  PSRAM_COPY_ERR_OPEN_FAIL = 2,
  PSRAM_COPY_ERR_EMPTY_OR_TOO_BIG = 3,
  PSRAM_COPY_ERR_PSRAM_ALLOC_FAIL = 4,
  PSRAM_COPY_ERR_READ_FAIL = 5,
  PSRAM_COPY_ERR_QUEUE_FAIL = 6,
};

typedef struct {
  void*    psram_ptr;   // nullptr on failure
  size_t   size;        // 0 on failure
  int      err;         // 0 on success, otherwise an error code you define
} PsramFileCopyResult_t;

typedef struct {
  const char* file_path;      // LittleFS path e.g. "/icons/foo.bin"
  QueueHandle_t result_queue; // queue where result is posted (length 1 is fine)
} PsramFileCopyRequest_t;

PsramFileCopyResult_t copy_file_littlefs_to_psram_blocking(const char* path, uint32_t timeout_ms = 5000);

static void copy_littlefs_file_to_psram_task(void* pv)
{
  PsramFileCopyRequest_t req = *(PsramFileCopyRequest_t*)pv;  // copy request
  vPortFree(pv); // free the heap-allocated task param (see starter function below)

  PsramFileCopyResult_t result{};
  result.psram_ptr = nullptr;
  result.size      = 0;
  result.err       = PSRAM_COPY_OK;

  if (!req.file_path || !req.result_queue) {
    result.err = PSRAM_COPY_ERR_BAD_ARG;
    xQueueSend(req.result_queue, &result, 0);
    vTaskDelete(NULL);
  }

  File f = LittleFS.open(req.file_path, "r");
  if (!f) {
    result.err = PSRAM_COPY_ERR_OPEN_FAIL;
    xQueueSend(req.result_queue, &result, 0);
    vTaskDelete(NULL);
  }

  size_t sz = (size_t)f.size();
  if (sz == 0 || sz > (size_t)UINT32_MAX) {
    // guard: empty file or absurdly huge (adjust limit as needed)
    f.close();
    result.err = PSRAM_COPY_ERR_EMPTY_OR_TOO_BIG;
    xQueueSend(req.result_queue, &result, 0);
    vTaskDelete(NULL);
  }

  // Allocate in PSRAM (8-bit addressable)
  void* buf = heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) {
    f.close();
    result.err = PSRAM_COPY_ERR_PSRAM_ALLOC_FAIL;
    xQueueSend(req.result_queue, &result, 0);
    vTaskDelete(NULL);
  }

  // Read fully
  size_t total = 0;
  while (total < sz) {
    int r = f.read((uint8_t*)buf + total, sz - total);
    if (r <= 0) {
      // read failed or EOF early
      heap_caps_free(buf);
      f.close();
      result.err = PSRAM_COPY_ERR_READ_FAIL;
      xQueueSend(req.result_queue, &result, 0);
      vTaskDelete(NULL);
    }
    total += (size_t)r;
  }

  f.close();

  // Success
  result.psram_ptr = buf;
  result.size      = sz;
  result.err       = PSRAM_COPY_OK;

  if (xQueueSend(req.result_queue, &result, pdMS_TO_TICKS(50)) != pdTRUE) {
    // If caller isn't listening, don’t leak PSRAM
    heap_caps_free(buf);
    result.psram_ptr = nullptr;
    result.size = 0;
    result.err = PSRAM_COPY_ERR_QUEUE_FAIL;
    // best effort (queue still might be full)
    xQueueSend(req.result_queue, &result, 0);
  }

  vTaskDelete(NULL);
}


PsramFileCopyResult_t copy_file_littlefs_to_psram_blocking(const char* path, uint32_t timeout_ms){
  PsramFileCopyResult_t out{};
  out.psram_ptr = nullptr;
  out.size = 0;
  out.err = PSRAM_COPY_ERR_BAD_ARG;

  if (!path) return out;

  QueueHandle_t q = xQueueCreate(1, sizeof(PsramFileCopyResult_t));
  if (!q) {
    out.err = PSRAM_COPY_ERR_QUEUE_FAIL;
    return out;
  }

  // Allocate task param on heap so it remains valid after we return from here
  PsramFileCopyRequest_t* req = (PsramFileCopyRequest_t*)pvPortMalloc(sizeof(PsramFileCopyRequest_t));
  if (!req) {
    vQueueDelete(q);
    out.err = PSRAM_COPY_ERR_QUEUE_FAIL;
    return out;
  }

  req->file_path = path;
  req->result_queue = q;

  // IMPORTANT for Pixlpal: this task reads flash/FS, so keep stack in internal RAM.
  // (Don’t create this task with stack-in-PSRAM options.)
  BaseType_t ok = xTaskCreate(
      copy_littlefs_file_to_psram_task,
      "lfs2psram",
      4096,     // stack words/bytes depends on config; 4096 bytes is usually safe here
      req,
      tskIDLE_PRIORITY + 2,
      nullptr
  );

  if (ok != pdPASS) {
    vPortFree(req);
    vQueueDelete(q);
    out.err = PSRAM_COPY_ERR_QUEUE_FAIL;
    return out;
  }

  if (xQueueReceive(q, &out, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
    // Timed out waiting. We can’t safely reclaim PSRAM here because we don’t know state.
    // In practice: choose a reasonable timeout and ensure task always sends.
    out.psram_ptr = nullptr;
    out.size = 0;
    out.err = PSRAM_COPY_ERR_QUEUE_FAIL;
  }

  vQueueDelete(q);
  return out;
}

// size_t drawPNGsCount = 0; 
// size_t drawSVGsCount = 0; 
//*************************************************************************************
void configure_ledc()
{
	ledc_timer_config_t ledc_timer = {
		.speed_mode = LEDC_LOW_SPEED_MODE,
		.duty_resolution = LEDC_TIMER_8_BIT,
		.timer_num = LEDC_TIMER_0,
		.freq_hz = 5000,
		.clk_cfg = LEDC_AUTO_CLK};
	ledc_timer_config(&ledc_timer);

	ledc_channel_config_t ledc_channel_r = {
		.gpio_num = RGB_LED_PIN_R,
		.speed_mode = LEDC_LOW_SPEED_MODE,
		.channel = LEDC_CHANNEL_0,
		.intr_type = LEDC_INTR_DISABLE,
		.timer_sel = LEDC_TIMER_0,
		.duty = 0,
		.hpoint = 0};
	ledc_channel_config(&ledc_channel_r);

	ledc_channel_config_t ledc_channel_g = {
		.gpio_num = RGB_LED_PIN_G,
		.speed_mode = LEDC_LOW_SPEED_MODE,
		.channel = LEDC_CHANNEL_1,
		.intr_type = LEDC_INTR_DISABLE,
		.timer_sel = LEDC_TIMER_0,
		.duty = 0,
		.hpoint = 0};
	ledc_channel_config(&ledc_channel_g);

	ledc_channel_config_t ledc_channel_b = {
		.gpio_num = RGB_LED_PIN_B,
		.speed_mode = LEDC_LOW_SPEED_MODE,
		.channel = LEDC_CHANNEL_2,
		.intr_type = LEDC_INTR_DISABLE,
		.timer_sel = LEDC_TIMER_0,
		.duty = 0,
		.hpoint = 0};
	ledc_channel_config(&ledc_channel_b);
}

void set_rgb_color(uint16_t color, uint8_t brightness)
{
	uint8_t red = (color >> 11) & 0x1F;
	uint8_t green = (color >> 5) & 0x3F;
	uint8_t blue = color & 0x1F;

	// Scale brightness
	red = (red * brightness) / 255;
	green = (green * brightness) / 255;
	blue = (blue * brightness) / 255;

	// Invert logic for common-anode
	ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 255 - red * 8); // 8-bit resolution
	ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

	ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 255 - green * 4); // 6-bit to 8-bit scaling
	ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

	ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 255 - blue * 8); // 5-bit to 8-bit scaling
	ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}

void rgb_led_task(void *param){
	rgb_led_message_t msg;
	while (1){
		if (xQueueReceive(rgb_led_queue, &msg, portMAX_DELAY)){
			set_rgb_color(msg.color, msg.brightness);
			currentStatusLEDcolor = msg.color;
		}
	}
}

void mtb_Set_Status_RGB_LED(uint16_t dColor, uint8_t dBrightness){
	rgb_led_message_t message = {.color = dColor, .brightness = dBrightness}; // Red color with half brightness
	xQueueSend(rgb_led_queue, &message, portMAX_DELAY);
}

void mtb_Draw_Status_Bar(void){
	if (Mtb_Applications::currentRunningApp->fullScreen == false){
		//ESP_LOGI(TAG, "Called Draw status bar...\n");
		for (uint8_t i = 0; i < 11; i++) if (statusBarItems[i].xAxis != 0 && statusBarItems[i].yAxis != 0) xQueueSend(pngLocalImageDrawer_Q, &statusBarItems[i], 0);
		mtb_Launch_This_Service(mtb_Png_Local_ImageDrawer_Sv);
		mtb_Panel_Draw_HLine(9, 0, PANEL_RES_X, mtb_Panel_Color565(35, 35, 35)); // Dark gray line
	}
}

//***PASSED ********************************************************************
void Mtb_Static_Text_t::mtb_Config_Disp_Panel_Pins(){
	pinMode(ALM_BUZZER, OUTPUT); // Make the alarm silent at the very start.
	digitalWrite(ALM_BUZZER, LOW);
	if (rgb_led_queue == NULL) rgb_led_queue = xQueueCreate(5, sizeof(rgb_led_message_t));
	configure_ledc();
	xTaskCreatePinnedToCore(rgb_led_task, "rgb_led_task", 2048, NULL, 5, NULL, 0);
	mtb_Set_Status_RGB_LED(BLACK);
}

// PASSED
void Mtb_Static_Text_t::mtb_Init_Led_Matrix_Panel(){
	uint8_t *pngLocalItems_buffer = (uint8_t *)heap_caps_malloc(20 * sizeof(Mtb_LocalImage_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	uint8_t *svgLocalItems_buffer = (uint8_t *)heap_caps_malloc(20 * sizeof(Mtb_LocalImage_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

	if (pngLocalImageDrawer_Q == NULL) pngLocalImageDrawer_Q = xQueueCreateStatic(20, sizeof(Mtb_LocalImage_t), pngLocalItems_buffer, &xQueueStorage_PNG_LocalDrawings);
	if (svgLocalImageDrawer_Q == NULL) svgLocalImageDrawer_Q = xQueueCreateStatic(20, sizeof(Mtb_LocalImage_t), svgLocalItems_buffer, &xQueueStorage_SVG_LocalDrawings);

	Hub75Config config{};
    config.panel_width = PANEL_RES_X;
    config.panel_height = PANEL_RES_Y;
    config.scan_pattern = Hub75ScanPattern::SCAN_1_32;
    config.scan_wiring = Hub75ScanWiring::STANDARD_TWO_SCAN;  // Most panels
    config.shift_driver = Hub75ShiftDriver::GENERIC;          // Or GENERIC
	config.min_refresh_rate = 120;                         // Set a minimum refresh rate to reduce flicker

    // Set GPIO pins
    config.pins.r1 = 4;
    config.pins.g1 = 5;
    config.pins.b1 = 6;
    config.pins.r2 = 7;
    config.pins.g2 = 15;
    config.pins.b2 = 16;
    config.pins.a = 18;
    config.pins.b = 8;
    config.pins.c = 9;
    config.pins.d = 10;
    config.pins.e = 17;
    config.pins.lat = 12;
    config.pins.oe = 13;
    config.pins.clk = 11;

	mtb_Display_Driver = new Hub75Driver(config);
    mtb_Display_Driver->begin();  // Starts continuous refresh

	mtb_Read_Nvs_Struct("pan_brghnss", &panelBrightness, sizeof(uint8_t));
	mtb_Panel_Set_Brightness(panelBrightness); // 0-255
	mtb_Set_Status_RGB_LED(currentStatusLEDcolor, panelBrightness);
	// if(litFS_Ready)drawGIF("/mtblg/mtbStart.gif",0,0,1);
}

//************************************************
Mtb_Static_Text_t::Mtb_Static_Text_t(int16_t x1, int16_t y1, const uint8_t *font)
{
	setfont(font);
	x1Seg = originX1Seg = x1;
	y1Seg = originY1Seg = y1;
}

void Mtb_Static_Text_t::setfont(const uint8_t *font)
{
	fontMain = (uint8_t *)font;
	yAxis = font[6];
	charSpacing = font[0];
}

//*************************************************************************************
void Mtb_FixedText_t::mtb_Clear_Panel_Segment(void)
{
	for (int16_t i = x1Seg < 0 ? 0 : x1Seg, p = x1Seg + textHorizSpace; i < p; i++)
	{
		for (int16_t j = y1Seg, q = y1Seg + yAxis; j < q; j++)
		{
			scratchPad[i][j] = 0;
		}
	}
}
//*************************************************************************************
void Mtb_FixedText_t::mtb_Update_Panel_Segment(void)
{
	for (int16_t i = x1Seg < 0 ? 0 : x1Seg, p = x1Seg + textHorizSpace; i < p; i++)
	{
		for (int16_t j = y1Seg < 0 ? 0 : y1Seg, q = y1Seg + yAxis; j < q; j++)
		{
			if (!(scratchPad[i][j])) mtb_Panel_Draw_Pixel565(i, j, backgroundColor); // update color
		}
	}
}
//**************************************************************************************
void Mtb_Static_Text_t::mtb_Clear_Screen(void)
{
	mtb_Panel_Clear_Screen();
}
//**************************************************************************************
// PASSED
void Mtb_FixedText_t::mtb_Set_Pixel_Data(int16_t xPs, int16_t yPs)
{
	mtb_Panel_Draw_Pixel565(xPs, yPs, color); // update color
	scratchPad[xPs][yPs] = 1;
}

//**************************************************************************************
// PASSED
void ScrollTextHelper_t::mtb_Set_Pixel_Data(int16_t xPs, int16_t yPs)
{
	scrollBuffer[xPs][yPs] = 1;
}
//**************************************************************************************
// write character function
void Mtb_Static_Text_t::writeXter(uint16_t character, int16_t xPixel, int16_t yPixel)
{
	uint16_t xOrigin = xPixel;
	uint8_t masker = 0;
	uint16_t locator = (character * 4) + 8;
	uint8_t rowCount = fontMain[locator];
	rowCount = (rowCount / 8) + ((rowCount % 8) > 0 ? 1 : 0);
	character = (fontMain[locator + 1]) + (fontMain[locator + 2] * 256);

	for (uint8_t i = 0; i < yAxis; ++i, ++yPixel, xPixel = xOrigin)
	{
		for (uint8_t p = 0; p < rowCount; ++p)
		{
			masker = fontMain[character++];
			for (uint8_t j = 0; j < 8; ++j, ++xPixel)
			{
				if (masker & 0x01)
					mtb_Set_Pixel_Data(xPixel, yPixel);
				masker >>= 1;
			}
		}
	}
}
//**************************************************************************************
uint16_t Mtb_Static_Text_t::mtb_Write_String(const char *myString){
	uint16_t charCount = strlen(myString);
	uint16_t row = x1Seg;
	uint16_t xBorder = 0;
	uint16_t position = textStyle ? y1Seg : 0;

	if(x1Seg < 0){
		Mtb_ScrollText_t moreCryptoData (0, y1Seg, PANEL_RES_X, fontMain, color, 20, 1);
		moreCryptoData.backgroundColor = backgroundColor;
		moreCryptoData.mtb_Scroll_This_Text(myString);
		ESP_LOGI(TAG, "Text position out of bounds. Text will scroll instead");
		return 0; // Out of bounds check
	}

	if (textStyle)mtb_Clear_Panel_Segment();

	for (uint16_t i = 0; i < charCount && xBorder < PANEL_RES_X; row += fontMain[(myString[i] - 32) * 4 + 8] + charSpacing, ++i, xBorder = textStyle ? (row + fontMain[(myString[i] - 32) * 4 + 8] + charSpacing) : 0){
	
		if (myString[i] > '~'){
			row -= fontMain[(myString[i] - 32) * 4 + 8] + charSpacing;
			continue;
		} // Filter out all Extended ASCII printable characters.

		switch (myString[i]){
		case ' ':
			row += fontMain[1];
			continue;
			//		case ' ': writeXter(0,row,position); continue;
		case '0':
			writeXter(16, row, position);
			continue;
		case '1':
			writeXter(17, row, position);
			continue;
		case '2':
			writeXter(18, row, position);
			continue;
		case '3':
			writeXter(19, row, position);
			continue;
		case '4':
			writeXter(20, row, position);
			continue;
		case '5':
			writeXter(21, row, position);
			continue;
		case '6':
			writeXter(22, row, position);
			continue;
		case '7':
			writeXter(23, row, position);
			continue;
		case '8':
			writeXter(24, row, position);
			continue;
		case '9':
			writeXter(25, row, position);
			continue;
		case 'a':
			writeXter(65, row, position);
			continue;
		case 'b':
			writeXter(66, row, position);
			continue;
		case 'c':
			writeXter(67, row, position);
			continue;
		case 'd':
			writeXter(68, row, position);
			continue;
		case 'e':
			writeXter(69, row, position);
			continue;
		case 'f':
			writeXter(70, row, position);
			continue;
		case 'g':
			writeXter(71, row, position);
			continue;
		case 'h':
			writeXter(72, row, position);
			continue;
		case 'i':
			writeXter(73, row, position);
			continue;
		case 'j':
			writeXter(74, row, position);
			continue;
		case 'k':
			writeXter(75, row, position);
			continue;
		case 'l':
			writeXter(76, row, position);
			continue;
		case 'm':
			writeXter(77, row, position);
			continue;
		case 'n':
			writeXter(78, row, position);
			continue;
		case 'o':
			writeXter(79, row, position);
			continue;
		case 'p':
			writeXter(80, row, position);
			continue;
		case 'q':
			writeXter(81, row, position);
			continue;
		case 'r':
			writeXter(82, row, position);
			continue;
		case 's':
			writeXter(83, row, position);
			continue;
		case 't':
			writeXter(84, row, position);
			continue;
		case 'u':
			writeXter(85, row, position);
			continue;
		case 'v':
			writeXter(86, row, position);
			continue;
		case 'w':
			writeXter(87, row, position);
			continue;
		case 'x':
			writeXter(88, row, position);
			continue;
		case 'y':
			writeXter(89, row, position);
			continue;
		case 'z':
			writeXter(90, row, position);
			continue;
		case 'A':
			writeXter(33, row, position);
			continue;
		case 'B':
			writeXter(34, row, position);
			continue;
		case 'C':
			writeXter(35, row, position);
			continue;
		case 'D':
			writeXter(36, row, position);
			continue;
		case 'E':
			writeXter(37, row, position);
			continue;
		case 'F':
			writeXter(38, row, position);
			continue;
		case 'G':
			writeXter(39, row, position);
			continue;
		case 'H':
			writeXter(40, row, position);
			continue;
		case 'I':
			writeXter(41, row, position);
			continue;
		case 'J':
			writeXter(42, row, position);
			continue;
		case 'K':
			writeXter(43, row, position);
			continue;
		case 'L':
			writeXter(44, row, position);
			continue;
		case 'M':
			writeXter(45, row, position);
			continue;
		case 'N':
			writeXter(46, row, position);
			continue;
		case 'O':
			writeXter(47, row, position);
			continue;
		case 'P':
			writeXter(48, row, position);
			continue;
		case 'Q':
			writeXter(49, row, position);
			continue;
		case 'R':
			writeXter(50, row, position);
			continue;
		case 'S':
			writeXter(51, row, position);
			continue;
		case 'T':
			writeXter(52, row, position);
			continue;
		case 'U':
			writeXter(53, row, position);
			continue;
		case 'V':
			writeXter(54, row, position);
			continue;
		case 'W':
			writeXter(55, row, position);
			continue;
		case 'X':
			writeXter(56, row, position);
			continue;
		case 'Y':
			writeXter(57, row, position);
			continue;
		case 'Z':
			writeXter(58, row, position);
			continue;
		case '[':
			writeXter(59, row, position);
			continue;
		case '\\':
			writeXter(60, row, position);
			continue;
		case ']':
			writeXter(61, row, position);
			continue;
		case '^':
			writeXter(62, row, position);
			continue;
		case '_':
			writeXter(63, row, position);
			continue;
		case '`':
			writeXter(PANEL_RES_Y, row, position);
			continue;
		case '!':
			writeXter(1, row, position);
			continue;
		case '\"':
			writeXter(2, row, position);
			continue;
		case '#':
			writeXter(3, row, position);
			continue;
		case '$':
			writeXter(4, row, position);
			continue;
		case '%':
			writeXter(5, row, position);
			continue;
		case '&':
			writeXter(6, row, position);
			continue;
		case '\'':
			writeXter(7, row, position);
			continue;
		case '(':
			writeXter(8, row, position);
			continue;
		case ')':
			writeXter(9, row, position);
			continue;
		case '*':
			writeXter(10, row, position);
			continue;
		case '+':
			writeXter(11, row, position);
			continue;
		case ',':
			writeXter(12, row, position);
			continue;
		case '-':
			writeXter(13, row, position);
			continue;
		case '.':
			writeXter(14, row, position);
			continue;
		case '/':
			writeXter(15, row, position);
			continue;
		case ':':
			writeXter(26, row, position);
			continue;
		case ';':
			writeXter(27, row, position);
			continue;
		case '<':
			writeXter(28, row, position);
			continue;
		case '=':
			writeXter(29, row, position);
			continue;
		case '>':
			writeXter(30, row, position);
			continue;
		case '?':
			writeXter(31, row, position);
			continue;
		case '@':
			writeXter(32, row, position);
			continue;
		case '{':
			writeXter(91, row, position);
			continue;
		case '|':
			writeXter(92, row, position);
			continue;
		case '}':
			writeXter(93, row, position);
			continue;
		case '~':
			writeXter(94, row, position);
			continue;
		default:
			continue; // Consider writing the # symbol if the alphabet or symbol is not found.
		}
	}

	if (textStyle) mtb_Update_Panel_Segment();
	textHorizSpace = row - x1Seg;
	return row;
}
//**************************************************************************************
uint16_t Mtb_Static_Text_t::mtb_Write_String(String myString){
	return mtb_Write_String(myString.c_str());
}
//**************************************************************************************
uint16_t Mtb_FixedText_t::mtb_Write_Colored_Text(const char *myString, uint16_t dColor){
	color = dColor;
	return mtb_Write_String(myString);
}
uint16_t Mtb_FixedText_t::mtb_Write_Colored_Text(String myString, uint16_t dColor){
	color = dColor;
	return mtb_Write_String(myString);
}
//**************************************************************************************
uint16_t Mtb_FixedText_t::mtb_Write_Colored_Text(const char *myString, uint16_t dColor, uint16_t dBackgroundColor){
	color = dColor;
	backgroundColor = dBackgroundColor;
	return mtb_Write_String(myString);
}

uint16_t Mtb_FixedText_t::mtb_Write_Colored_Text(String myString, uint16_t dColor, uint16_t dBackgroundColor){
	color = dColor;
	backgroundColor = dBackgroundColor;
	return mtb_Write_String(myString);
}
//*************************************************************************************
void Mtb_CentreText_t::mtb_Clear_Panel_Segment(void){
	for (int16_t i = x1Seg < 0 ? 0 : x1Seg, p = x1Seg + textHorizSpace; i < p; i++)
	{
		for (int16_t j = y1Seg, q = y1Seg + yAxis; j < q; j++)
		{
			mtb_Panel_Draw_Pixel565(i, j, backgroundColor); // update color
		}
	}
}
//**************************************************************************************
uint16_t Mtb_CentreText_t::mtb_Write_String(const char *myString)
{
	mtb_Clear_Panel_Segment();

	uint16_t charCount = strlen(myString);
	uint16_t row = 0;

	for (uint16_t i = 0; i < charCount; row += fontMain[(myString[i] - 32) * 4 + 8] + charSpacing, ++i)
	{
		if (myString[i] > '~')
		{
			row -= fontMain[(myString[i] - 32) * 4 + 8] + charSpacing;
		} // Filter out all Extended ASCII printable characters.
	}

	x1Seg = originX1Seg - row / 2;
	y1Seg = originY1Seg - yAxis / 2;

	return Mtb_FixedText_t::mtb_Write_String(myString);
}
//**************************************************************************************
uint16_t Mtb_CentreText_t::mtb_Write_String(String myString)
{
	mtb_Clear_Panel_Segment();

	uint16_t charCount = strlen(myString.c_str());
	uint16_t row = 0;

	for (uint16_t i = 0; i < charCount; row += fontMain[(myString.c_str()[i] - 32) * 4 + 8] + charSpacing, ++i)
	{
		if (myString.c_str()[i] > '~')
		{
			row -= fontMain[(myString.c_str()[i] - 32) * 4 + 8] + charSpacing;
		} // Filter out all Extended ASCII printable characters.
	}

	x1Seg = originX1Seg - row / 2;
	y1Seg = originY1Seg - yAxis / 2;

	return Mtb_FixedText_t::mtb_Write_String(myString.c_str());
}
//**************************************************************************************
uint16_t Mtb_CentreText_t::mtb_Write_Colored_Text(const char *myString, uint16_t dColor)
{
	color = dColor;
	mtb_Clear_Panel_Segment();

	uint16_t charCount = strlen(myString);
	uint16_t row = 0;

	for (uint16_t i = 0; i < charCount; row += fontMain[(myString[i] - 32) * 4 + 8] + charSpacing, ++i)
	{
		if (myString[i] > '~')
		{
			row -= fontMain[(myString[i] - 32) * 4 + 8] + charSpacing;
		} // Filter out all Extended ASCII printable characters.
	}

	x1Seg = originX1Seg - row / 2;
	y1Seg = originY1Seg - yAxis / 2;

	return Mtb_FixedText_t::mtb_Write_String(myString);
}
//**************************************************************************************
uint16_t Mtb_CentreText_t::mtb_Write_Colored_Text(String myString, uint16_t dColor)
{
	color = dColor;
	mtb_Clear_Panel_Segment();

	uint16_t charCount = strlen(myString.c_str());
	uint16_t row = 0;

	for (uint16_t i = 0; i < charCount; row += fontMain[(myString.c_str()[i] - 32) * 4 + 8] + charSpacing, ++i)
	{
		if (myString.c_str()[i] > '~')
		{
			row -= fontMain[(myString.c_str()[i] - 32) * 4 + 8] + charSpacing;
		} // Filter out all Extended ASCII printable characters.
	}

	x1Seg = originX1Seg - row / 2;
	y1Seg = originY1Seg - yAxis / 2;

	return Mtb_FixedText_t::mtb_Write_String(myString.c_str());
}
//**************************************************************************************
uint16_t Mtb_CentreText_t::mtb_Write_Colored_Text(const char *myString, uint16_t dColor, uint16_t dBackgroundColor){
	color = dColor;
	backgroundColor = dBackgroundColor;
	mtb_Clear_Panel_Segment();

	uint16_t charCount = strlen(myString);
	uint16_t row = 0;

	for (uint16_t i = 0; i < charCount; row += fontMain[(myString[i] - 32) * 4 + 8] + charSpacing, ++i){
		if (myString[i] > '~')
		{
			row -= fontMain[(myString[i] - 32) * 4 + 8] + charSpacing;
		} // Filter out all Extended ASCII printable characters.
	}

	x1Seg = originX1Seg - row / 2;
	y1Seg = originY1Seg - yAxis / 2;

	return Mtb_FixedText_t::mtb_Write_String(myString);
}
//**************************************************************************************
uint16_t Mtb_CentreText_t::mtb_Write_Colored_Text(String myString, uint16_t dColor, uint16_t dBackgroundColor){
	color = dColor;
	backgroundColor = dBackgroundColor;
	mtb_Clear_Panel_Segment();

	uint16_t charCount = strlen(myString.c_str());
	uint16_t row = 0;

	for (uint16_t i = 0; i < charCount; row += fontMain[(myString.c_str()[i] - 32) * 4 + 8] + charSpacing, ++i){
		if (myString.c_str()[i] > '~')
		{
			row -= fontMain[(myString.c_str()[i] - 32) * 4 + 8] + charSpacing;
		} // Filter out all Extended ASCII printable characters.
	}

	x1Seg = originX1Seg - row / 2;
	y1Seg = originY1Seg - yAxis / 2;

	return Mtb_FixedText_t::mtb_Write_String(myString.c_str());
}
//**************************************************************************************
uint16_t Mtb_FixedText_t::mtb_Clear_String()
{
	color = backgroundColor;
	Mtb_Static_Text_t::mtb_Write_String(".");
	return 0;
}
//************************************************************************************ */
void mtb_Draw_Local_Png_Task(void *dService){
	//ESP_LOGI(TAG, "PNG Local Image Drawer Task Started\n");
	Mtb_Services *THIS_SERVICE = (Mtb_Services *)dService;
	Mtb_LocalImage_t holderItem;
	unsigned error;
	unsigned char *image = 0;
	unsigned int width, height;

	while ((xQueueReceive(pngLocalImageDrawer_Q, &holderItem, pdMS_TO_TICKS(200)) == pdTRUE) && (litFS_Ready == true)){
		//printf("Drawing status bar from %s\n", holderItem.imagePath);
		// Check if the file exists, if not, download the file if internet connection exists.
		// xQueuePeek(pngLocalImageDrawer_Q, &holderItem, pdMS_TO_TICKS(100));
		if (!LittleFS.exists(holderItem.imagePath)){
			bool dwnld_Succeed = false;
			ESP_LOGI(TAG, "PNG File Does Not Exist\n");
			if (Mtb_Applications::internetConnectStatus == true){
				ESP_LOGI(TAG, "Downloading the file......\n");
				dwnld_Succeed = mtb_Download_Github_Strg_File(String(holderItem.imagePath + 1), String(holderItem.imagePath));
			}else{
				File2Download_t dFile;
				memcpy(dFile.flashFilePath, holderItem.imagePath, 50);
				memcpy(dFile.githubFilePath, holderItem.imagePath + 1, 50);
				xQueueSend(files2Download_Q, &dFile, pdMS_TO_TICKS(1));
				ESP_LOGI(TAG, "File Queued for later downloading.\n");
			}
			// xQueueReceive(pngLocalImageDrawer_Q, &holderItem, pdMS_TO_TICKS(100));
			if (dwnld_Succeed == false) continue; // Skip to the next item in the queue if the file does not exist.
		}

			//String imageFilePath = "/littlefs" + String(holderItem.imagePath);
			PsramFileCopyResult_t r = copy_file_littlefs_to_psram_blocking(holderItem.imagePath, 5000);
			
			//error = lodepng_decode24_file(&image, &width, &height, imageFilePath.c_str());
			error = lodepng_decode24(&image, &width, &height, (const unsigned char*)r.psram_ptr, (size_t)r.size);
			if (error) {
				ESP_LOGI(TAG, "error %u: %s\n", error, lodepng_error_text(error));
				free(image);
				heap_caps_free(r.psram_ptr);
				continue;
			}

			// Start drawing with anti-aliased downscaling
			uint8_t scaleFactor = holderItem.scale;
			if (scaleFactor < 1) scaleFactor = 1;

			uint16_t scaledWidth = width / scaleFactor;
			uint16_t scaledHeight = height / scaleFactor;

			if (scaledWidth < 129 && scaledHeight < 65) {
				for (uint16_t yOut = 0; yOut < scaledHeight; yOut++) {
					for (uint16_t xOut = 0; xOut < scaledWidth; xOut++) {

						uint32_t rSum = 0, gSum = 0, bSum = 0;
						uint16_t pixelCount = 0;

						// Loop through the block of source pixels to average
						for (uint8_t dy = 0; dy < scaleFactor; dy++) {
							for (uint8_t dx = 0; dx < scaleFactor; dx++) {
								uint16_t xIn = xOut * scaleFactor + dx;
								uint16_t yIn = yOut * scaleFactor + dy;

								if (xIn >= width || yIn >= height) continue;

								uint32_t idx = yIn * width * 3 + xIn * 3;
								rSum += image[idx + 0];
								gSum += image[idx + 1];
								bSum += image[idx + 2];
								pixelCount++;
							}
						}

						// Compute average color
						if (pixelCount > 0) {
							uint8_t rAvg = rSum / pixelCount;
							uint8_t gAvg = gSum / pixelCount;
							uint8_t bAvg = bSum / pixelCount;

							mtb_Panel_Draw_PixelRGB(holderItem.xAxis + xOut, holderItem.yAxis + yOut, rAvg, gAvg, bAvg);
						}
					}
				}
				free(image);
			} else {
			// ESP_LOGI(TAG, "Image Dimensions are: Width = %d; Height = %d\n", width, height);
			free(image);
			}
		heap_caps_free(r.psram_ptr);
	}

	mtb_Delete_This_Service(THIS_SERVICE);
}

BaseType_t mtb_Draw_Local_Png(const Mtb_LocalImage_t &dImage){
	xQueueSend(pngLocalImageDrawer_Q, &dImage, 0);
	mtb_Launch_This_Service(mtb_Png_Local_ImageDrawer_Sv);
	return 0;
}

// --- helpers ---------------------------------------------------------------

static const char* TAG_SVG = "SVG_DRAW";
static constexpr float DPI = 96.0f;

static inline uint16_t pack_rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static inline void over_black(uint8_t& r, uint8_t& g, uint8_t& b, uint8_t a) {
  if (a == 255) return;
  r = (uint8_t)((r * a) / 255);
  g = (uint8_t)((g * a) / 255);
  b = (uint8_t)((b * a) / 255);
}

static char* load_text_file_psram(const char* path, size_t& out_size) {
  FILE* f = fopen(path, "rb");
  if (!f) { ESP_LOGE(TAG_SVG, "fopen failed: %s", path); return nullptr; }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz <= 0) { fclose(f); ESP_LOGE(TAG_SVG, "file size invalid"); return nullptr; }

  char* buf = (char*)heap_caps_malloc((size_t)sz + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) { fclose(f); ESP_LOGE(TAG_SVG, "PSRAM alloc failed (%ld bytes)", sz + 1); return nullptr; }

  size_t rd = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  if (rd != (size_t)sz) { ESP_LOGE(TAG_SVG, "fread short"); heap_caps_free(buf); return nullptr; }
  buf[sz] = '\0';
  out_size = (size_t)sz;
  return buf;
}


//************************************************************************************ */
void mtb_Draw_Local_Svg_Task(void *dService){
	ESP_LOGI(TAG, "SVG Local Image Drawer Task Started\n");
	Mtb_Services *THIS_SERVICE = (Mtb_Services *)dService;
	Mtb_LocalImage_t holderItem;
	unsigned error;
	unsigned char *image = 0;
	unsigned int width, height;
	uint16_t colorHolder = 0;

	while ((xQueueReceive(svgLocalImageDrawer_Q, &holderItem, pdMS_TO_TICKS(100)) == pdTRUE) && (litFS_Ready == true)){

		if (!LittleFS.exists(holderItem.imagePath)){
			bool dwnld_Succeed = false;
			ESP_LOGI(TAG, "SVG File Does Not Exist\n");
			if (Mtb_Applications::internetConnectStatus == true){
				ESP_LOGI(TAG, "Downloading the file......\n");
				dwnld_Succeed = mtb_Download_Github_Strg_File(String(holderItem.imagePath + 1), String(holderItem.imagePath));
			}else{
				File2Download_t dFile;
				memcpy(dFile.flashFilePath, holderItem.imagePath, 50);
				memcpy(dFile.githubFilePath, holderItem.imagePath + 1, 50);
				xQueueSend(files2Download_Q, &dFile, pdMS_TO_TICKS(1));
				ESP_LOGI(TAG, "File Queued for later downloading.\n");
			}
			if (dwnld_Succeed == false) continue; // Skip to the next item in the queue if the file does not exist.
		}

		// SVG RENDERING PATH     ??????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
		// build path
		String svgPath = "/littlefs" + String(holderItem.imagePath);

		// 1) load SVG XML into PSRAM
		size_t svg_size = 0;
		char* svg_text = load_text_file_psram(svgPath.c_str(), svg_size);
		if (!svg_text) continue;

		// 2) parse SVG
		NSVGimage* image = nsvgParse(svg_text, "px", DPI);
		if (!image) {
			ESP_LOGE(TAG_SVG, "nsvgParse failed");
			heap_caps_free(svg_text);
			continue;
		}

		// 3) FIT-TO-PANEL SCALING GUARD (your behavior)
		//    - When scale==1: fit to 128x64 (keep aspect)
		//    - When scale>1 : further downscale by 1/scale
		const float maxW = 128.0f, maxH = 64.0f;
		float svgW = (image->width  > 0.f) ? image->width  : 1024.0f;
		float svgH = (image->height > 0.f) ? image->height :  512.0f;

		uint8_t userScale = holderItem.scale < 1 ? 1 : holderItem.scale;

		float baseScaleX = maxW / svgW;
		float baseScaleY = maxH / svgH;
		float baseScale  = fminf(baseScaleX, baseScaleY);     // fit inside 128x64
		float scale      = baseScale / (float)userScale;      // extra downscale if userScale>1
		if (scale <= 0.0f) scale = 1.0f / (float)userScale;   // safety fallback

		// Output dimensions (rounded) and clamp to bounds
		uint16_t outW = (uint16_t)(svgW * scale + 0.5f);
		uint16_t outH = (uint16_t)(svgH * scale + 0.5f);
		if (outW == 0) outW = 1;
		if (outH == 0) outH = 1;
		if (outW > PANEL_RES_X) outW = PANEL_RES_X;
		if (outH > PANEL_RES_Y)  outH = PANEL_RES_Y;

		// 4) allocate RGBA8888 buffer in PSRAM for the scaled output
		const int    stride = outW * 4;              // bytes per row
		const size_t buf_bytes = (size_t)outH * stride;
		uint8_t* rgba = (uint8_t*)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
		if (!rgba) {
			ESP_LOGE(TAG_SVG, "PSRAM alloc for RGBA failed (%u bytes)", (unsigned)buf_bytes);
			nsvgDelete(image);
			heap_caps_free(svg_text);
			continue;
		}
		memset(rgba, 0, buf_bytes);

		// 5) rasterize at computed scale (top-left aligned)
		NSVGrasterizer* rast = nsvgCreateRasterizer();
		if (!rast) {
			ESP_LOGE(TAG_SVG, "nsvgCreateRasterizer failed");
			heap_caps_free(rgba);
			nsvgDelete(image);
			heap_caps_free(svg_text);
			continue;
		}

		nsvgRasterize(rast, image, 0.0f, 0.0f, scale, rgba, (int)outW, (int)outH, stride);

		// 6) push to display (alpha threshold like your PNG path)
		for (uint16_t y = 0; y < outH; ++y) {
			const uint8_t* row = rgba + y * stride;
			for (uint16_t x = 0; x < outW; ++x) {
			uint8_t a = row[x*4 + 3];
			if (a < PANEL_RES_X) continue; // keep whatever is already on the backbuffer

			uint8_t r = row[x*4 + 0];
			uint8_t g = row[x*4 + 1];
			uint8_t b = row[x*4 + 2];

			mtb_Panel_Draw_PixelRGB(holderItem.xAxis + x, holderItem.yAxis + y, r, g, b);
			}
		}

		// 7) cleanup
		nsvgDeleteRasterizer(rast);
		heap_caps_free(rgba);
		nsvgDelete(image);
		heap_caps_free(svg_text);

		// END OF SVG RENDERING     ??????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
	}

	mtb_Delete_This_Service(THIS_SERVICE);
}

BaseType_t mtb_Draw_Local_Svg(const Mtb_LocalImage_t &dImage){
	xQueueSend(svgLocalImageDrawer_Q, &dImage, 0);
	mtb_Launch_This_Service(mtb_SvgLocal_ImageDrawer_Sv);
	return 0;
}

//***************************************************************************************** */
void drawDecodedPNG(Mtb_PreloadedImage_t& pImg) {
    uint8_t* imageData = nullptr;
    unsigned width, height;
    unsigned error = lodepng_decode24(&imageData, &width, &height, pImg.imageBuffer, pImg.imageSize);

    if (error || !imageData) {
        ESP_LOGI(TAG, "LodePNG error %u: %s\n", error, lodepng_error_text(error));
        return;
    }

    uint8_t scale = pImg.meta.scale < 1 ? 1 : pImg.meta.scale;
    uint16_t scaledW = width / scale;
    uint16_t scaledH = height / scale;

    for (uint16_t yOut = 0; yOut < scaledH; yOut++) {
        for (uint16_t xOut = 0; xOut < scaledW; xOut++) {
            uint32_t rSum = 0, gSum = 0, bSum = 0;
            uint16_t pxCount = 0;

            for (uint8_t dy = 0; dy < scale; dy++) {
                for (uint8_t dx = 0; dx < scale; dx++) {
                    uint16_t xIn = xOut * scale + dx;
                    uint16_t yIn = yOut * scale + dy;
                    if (xIn >= width || yIn >= height) continue;

                    uint32_t idx = yIn * width * 3 + xIn * 3;
                    rSum += imageData[idx + 0];
                    gSum += imageData[idx + 1];
                    bSum += imageData[idx + 2];
                    pxCount++;
                }
            }

            if (pxCount > 0) {
                uint8_t r = rSum / pxCount;
                uint8_t g = gSum / pxCount;
                uint8_t b = bSum / pxCount;

                mtb_Panel_Draw_PixelRGB(pImg.meta.xAxis + xOut, pImg.meta.yAxis + yOut, r, g, b);
            }
        }
    }

    free(imageData);
}
//************************************************************************************ */

void pngDownloaderWorker(void* param) {
    size_t drawPNGsCount = (size_t)param;
	//log_i("PNG Downloader Worker started processing %d images", drawPNGsCount);
    while (true) {
        int myIndex;

        // Atomically fetch next index
        xSemaphoreTake(preloadIndexMutex, portMAX_DELAY);
        myIndex = preloadIndex++;
        xSemaphoreGive(preloadIndexMutex);

        if (myIndex >= drawPNGsCount) {
            break;
        }

        Mtb_PreloadedImage_t* img = &preloadedPNGs[myIndex];
        String mime;
        if (mtb_Download_Png_Img_To_PSRAM(img->meta.imageLink, &img->imageBuffer, &img->imageSize, &mime)) {
            img->isReady = true;
        } else {
            img->failed = true;
        }
		downloadedPNGs++;
    }
	
	while(downloadedPNGs < drawPNGsCount)delay(10); // Wait for all images to be processed
    //log_i("PNG Downloader Worker finished processing %d images", drawPNGsCount);
    vTaskDelete(NULL);  // ✅ Frees stack after work
}

void pngDrawerWorker(void* param) {
    size_t drawPNGsCount = (size_t)param;
	//log_i("PNG Drawer Worker started processing %d images", drawPNGsCount);
	// Now decode & draw
    for (int i = 0; i < drawPNGsCount; ++i){
        if (preloadedPNGs[i].isReady) {
			preloadedPNGs[i].isReady = false;
            drawDecodedPNG(preloadedPNGs[i]);
            free(preloadedPNGs[i].imageBuffer);
        }
    }
	//log_i("PNG Drawer Worker finished processing %d images", drawPNGsCount);
    vTaskDelete(NULL);  // ✅ Frees stack after work
}

bool mtb_Draw_Multi_Png(size_t drawPNGsCount, ImgWipeFn_ptr wipePreviousImg) {

	uint16_t imageDrawElapseTimer = 6000;
	TaskHandle_t h = NULL;

	while(downloadedPNGs < drawPNGsCount &&  imageDrawElapseTimer --> 0) delay(10); // Wait 1 minute for all images to be fetched from the internet
	if (downloadedPNGs < drawPNGsCount){
	ESP_LOGI(TAG,"Online PNG Download and Draw Timed-Out.\n");
	return false;
	} 
	
	wipePreviousImg();

	    // Start download worker tasks (e.g. 2 workers)
    for (int i = 0; i < drawPNGsCount; ++i) {
        xTaskCreatePinnedToCore(pngDrawerWorker, "PNG IMG_DR", 4096, (void*)drawPNGsCount, 1, NULL, 1);
				if (h == NULL) {
			ESP_LOGE(TAG, "Failed to create PNG drawer worker task");
			continue;
		}
		delay(1); // This delay is necessary to allow the task to start ahead of the next, so more than one task doesn't an image more than once.
    }

	downloadedPNGs = 0;
	return true;
}

void mtb_Download_Multi_Png(const Mtb_OnlineImage_t* images, size_t count) {
    if (count > MAX_PNG_IMAGES) count = MAX_PNG_IMAGES;
	preloadIndex = 0;

    // Prepare preload structures
    for (size_t i = 0; i < count; i++) {
        preloadedPNGs[i].meta = images[i];
        preloadedPNGs[i].imageBuffer = nullptr;
        preloadedPNGs[i].imageSize = 0;
        preloadedPNGs[i].isReady = false;
        preloadedPNGs[i].failed = false;
    }

    // Start download worker tasks (e.g. 2 workers)
    for (int i = 0; i < 2; ++i) {
        xTaskCreatePinnedToCore(pngDownloaderWorker, "IMG_DL", 8192, (void*)count, 1, NULL, 1);
    }
}

bool mtb_Draw_Online_Png(const Mtb_OnlineImage_t* images, size_t drawPNGsCount, ImgWipeFn_ptr wipePreviousImgs){
	    mtb_Download_Multi_Png(images, drawPNGsCount);
        return mtb_Draw_Multi_Png(drawPNGsCount, wipePreviousImgs);
}


//**************************************************************************************
void drawDecodedSVG(Mtb_PreloadedImage_t& item) {
    NSVGimage* svg = nsvgParse((char*)item.imageBuffer, "px", 96.0f);
    if (!svg) return;

    float svgW = svg->width > 0 ? svg->width : 1024.0f;
    float svgH = svg->height > 0 ? svg->height : 512.0f;

    const float maxW = 128.0f;
    const float maxH = 64.0f;

    uint8_t userScale = item.meta.scale < 1 ? 1 : item.meta.scale;
    float baseScaleX = maxW / svgW;
    float baseScaleY = maxH / svgH;
    float baseScale = fminf(baseScaleX, baseScaleY);
    float scale = baseScale / userScale;

    uint16_t outW = svgW * scale;
    uint16_t outH = svgH * scale;

    uint8_t* bitmap = (uint8_t*)ps_malloc(outW * outH * 4);
    if (!bitmap) {
        nsvgDelete(svg);
        return;
    }

    NSVGrasterizer* rast = nsvgCreateRasterizer();
    nsvgRasterize(rast, svg, 0, 0, scale, bitmap, outW, outH, outW * 4);
    nsvgDeleteRasterizer(rast);
    nsvgDelete(svg);

    for (uint16_t y = 0; y < outH; y++) {
        for (uint16_t x = 0; x < outW; x++) {
            uint32_t i = (y * outW + x) * 4;
            uint8_t r = bitmap[i + 0];
            uint8_t g = bitmap[i + 1];
            uint8_t b = bitmap[i + 2];
            uint8_t a = bitmap[i + 3];

            if (a < PANEL_RES_X) continue;

            mtb_Panel_Draw_PixelRGB(item.meta.xAxis + x, item.meta.yAxis + y, r, g, b);
        }
    }

    free(bitmap);
}

//**************************************************************************************
void svgDownloaderWorker(void* param) {
    size_t drawSVGsCount = (size_t)param;
    int myIndex;
	//log_i("SVG Downloader Worker started processing %d images", drawSVGsCount);
    while (true) {
        // Atomically grab next available index
        xSemaphoreTake(preloadIndexMutex, portMAX_DELAY);
        if (preloadIndex >= drawSVGsCount) {
            xSemaphoreGive(preloadIndexMutex);
            break;
        }
        myIndex = preloadIndex++;
        xSemaphoreGive(preloadIndexMutex);

        // Proceed to download this image
        Mtb_PreloadedImage_t* img = &preloadedSVGs[myIndex];
        String mime;
        if (mtb_Download_Svg_Img_To_PSRAM(img->meta.imageLink, &img->imageBuffer, &img->imageSize, &mime)) {
            img->isReady = true;
        } else {
            img->failed = true;
        }
		downloadedSVGs++;
    }
	//log_i("SVG Downloader Worker finished processing %d images", drawSVGsCount);
	while(downloadedSVGs < drawSVGsCount)delay(10); // Wait for all images to be processed

    vTaskDelete(NULL);
}

void svgDrawerWorker(void* param) {
	// param is a pointer to StaticTaskParams which carries stack/tcb ptrs
	size_t* args = (size_t*)param;
	size_t drawSVGsCount = args ? *args : 0;
	//log_i("SVG Drawer Worker started processing %d images", drawSVGsCount);
	// Now decode & draw
	for (int i = 0; i < drawSVGsCount; ++i){
		if (preloadedSVGs[i].isReady) {
			preloadedSVGs[i].isReady = false;
			drawDecodedSVG(preloadedSVGs[i]);
			free(preloadedSVGs[i].imageBuffer);
		}
	}
	//log_i("SVG Drawer Worker finished processing %d images", drawSVGsCount);
	vTaskDelete(NULL);  // ✅ Frees stack after work (actual memory freed asynchronously)
}

bool mtb_Draw_Multi_Svg(size_t drawSVGsCount, ImgWipeFn_ptr wipePreviousImg) {
	uint16_t imageDrawElapseTimer = 6000;
	TaskHandle_t h = NULL;

	while(downloadedSVGs < drawSVGsCount &&  imageDrawElapseTimer --> 0) delay(10); // Wait 1 minute for all images to be fetched from the internet
	if (downloadedSVGs < drawSVGsCount){
		ESP_LOGI(TAG,"Online SVG Download and Draw Timed-Out.\n");
		return false;
	} 

	wipePreviousImg();

	    // Start download worker tasks (e.g. 2 workers)
    for (int i = 0; i < drawSVGsCount; ++i) {
		xTaskCreatePinnedToCore(svgDrawerWorker, "SVG IMG_DR", 4096, &drawSVGsCount, 1, &h, 1);
		if (h == NULL) {
			ESP_LOGE(TAG, "Failed to create SVG drawer worker task");
			continue;
		}
		delay(1); // This delay is necessary to allow one task start ahead of the next, so two or more tasks doesn't draw the same image at the same time.
    }

	downloadedSVGs = 0;
	return true;
}

void mtb_Download_Multi_Svg(const Mtb_OnlineImage_t* images, size_t count) {
    if (count > MAX_SVG_IMAGES) count = MAX_SVG_IMAGES;
	preloadIndex = 0;
	downloadedSVGs = 0;

    for (size_t i = 0; i < count; i++) {
        preloadedSVGs[i].meta = images[i];
        preloadedSVGs[i].imageBuffer = nullptr;
        preloadedSVGs[i].imageSize = 0;
        preloadedSVGs[i].isReady = false;
        preloadedSVGs[i].failed = false;
    }

    for (int i = 0; i < 2; ++i) {
        xTaskCreatePinnedToCore(svgDownloaderWorker, "SVG_DL", 8192, (void*)count, 3, NULL, 0);
    }
}

bool mtb_Draw_Online_Svg(const Mtb_OnlineImage_t* images, size_t drawSVGsCount, ImgWipeFn_ptr wipePreviousImgs){
	mtb_Download_Multi_Svg(images, drawSVGsCount);
    return mtb_Draw_Multi_Svg(drawSVGsCount, wipePreviousImgs);
}


uint8_t mtb_Draw_Local_Gif(const Mtb_LocalAnim_t &dAnim){
	if(litFS_Ready != pdTRUE) return 1;		// No reading from littlefs if the otaStorage was not successful.
	String imageFilePath = "/littlefs" + String(dAnim.imagePath);
	uint16_t width, height;
	gd_GIF *gif = gd_open_gif(imageFilePath.c_str());
	width = gif->width;
	height = gif->height;
	if (width < 129 && height < 65){
		uint8_t *buffer = (uint8_t *)heap_caps_malloc(width * height * 3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
		//uint8_t *buffer = (uint8_t *)heap_caps_calloc(width * height * 3,sizeof(uint8_t), MALLOC_CAP_SPIRAM);
		for (uint32_t looped = 0; looped < dAnim.loopCount; looped++)
		{
			while (gd_get_frame(gif))
			{
				gd_render_frame(gif, buffer);

				// Draw the frame to the display
				mtb_Panel_Draw_FrameRGB888(dAnim.xAxis, dAnim.yAxis, width, height, buffer);

				// for (uint8_t p = 0, x = dAnim.xAxis; p < width; p++, x++){
				// 	for (uint8_t q = 0, y = dAnim.yAxis; q < height; q++, y++){
				// 		mtb_Panel_Draw_PixelRGB(x, y, buffer[q*width*3 + p*3], buffer[q*width*3 + p*3 + 1], buffer[q*width*3 + p*3 + 2]);
				// 	}
				// }
				
				vTaskDelay(pdMS_TO_TICKS(gif->gce.delay * 10));
			}
			gd_rewind(gif);
		}
		free(buffer);
		return 0;
	}
	else
		//printf("Gif Dimensions is: Width = %d; Height = %d;\n", width, height);
	gd_close_gif(gif);
	return 1;
}

//**************************************************************************************
void mtb_Show_Status_Bar_Icon(const Mtb_LocalImage_t &pngImage)
{
	uint8_t itemIndex = pngImage.xAxis / 9;
	memcpy(&statusBarItems[itemIndex], &pngImage, sizeof(Mtb_LocalImage_t));
	if (Mtb_Applications::currentRunningApp->fullScreen == false)
		mtb_Draw_Local_Png(pngImage);
}

void mtb_Wipe_Status_Bar_Icon(const Mtb_LocalImage_t &pngImage)
{
	mtb_Show_Status_Bar_Icon({"/batIcons/wipe7x7.png", pngImage.xAxis, pngImage.yAxis});
}
//**************************************************************************************

// Function to extract flag URL
String mtb_Get_Flag_Url_By_Country_Name(const String& countryName, const String& flagType) {
  const size_t CAPACITY = 150 * 1024; // ~150 KB, adjust to fit your JSON
  SpiRamJsonDocument doc(CAPACITY);

  // Parse
  auto err = deserializeJson(doc, jsonWorldCountries);
  if (err) {
    ESP_LOGI(TAG, "JSON parse error: %s \n", err.c_str());
    return String();
  }

  // Search for the matching country
  for (JsonObject country : doc.as<JsonArray>()) {
    const char* name = country["name"];
    if (name && countryName.equalsIgnoreCase(name)) {
      const char* url = country[flagType.c_str()];
      return url ? String(url) : String();
    }
  }

  return String(); // Not found or bad flagType
}


uint16_t mtb_Panel_Color565(uint8_t r, uint8_t g, uint8_t b, uint16_t *color){
	uint16_t packedColor = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
	if (color) *color = packedColor;
	return packedColor;
}

uint16_t mtb_Panel_Color32bit_To_Color565(const char* dartColor, uint16_t *color565){
	dartColor += 4; // skip "0xFF"
	uint8_t r = ((uint8_t)((strtol(dartColor,NULL,16) >> 16)));
	uint8_t g = ((uint8_t)((strtol(dartColor,NULL,16) >> 8)));
	uint8_t b = ((uint8_t)((strtol(dartColor,NULL,16) >> 0)));
	return mtb_Panel_Color565(r, g, b, color565);
}

void mtb_Panel_Draw_PixelRGB(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b){
	uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
	mtb_Panel_Frame_Buffer[x][y] = rgb565; // Update backbuffer
  	mtb_Display_Driver->draw_pixels(x, y, 1, 1, (uint8_t *)(&rgb565), Hub75PixelFormat::RGB565, Hub75ColorOrder::RGB, false);
}

void mtb_Panel_Draw_Pixel565(int16_t x, int16_t y, uint16_t color){
	mtb_Panel_Frame_Buffer[x][y] = color; // Update backbuffer
  	mtb_Display_Driver->draw_pixels(x, y, 1, 1, (uint8_t *)(&color), Hub75PixelFormat::RGB565, Hub75ColorOrder::RGB, false);
}

void mtb_Panel_Draw_FrameRGB888(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t *data){
	// Update backbuffer
	for (int16_t j = 0; j < height; j++) {
		for (int16_t i = 0; i < width; i++) {
			uint16_t color = ((data[(j * width + i) * 3 + 0] & 0xF8) << 8) | 
							 ((data[(j * width + i) * 3 + 1] & 0xFC) << 3) | 
							 (data[(j * width + i) * 3 + 2] >> 3);
			mtb_Panel_Frame_Buffer[x + i][y + j] = color;
		}
	}

	mtb_Display_Driver->draw_pixels(x, y, width, height, data, Hub75PixelFormat::RGB888, Hub75ColorOrder::RGB, false);
}


void mtb_Panel_Draw_FrameRGB565(int16_t x, int16_t y, int16_t width, int16_t height, const uint16_t *data){
    // Update backbuffer from 16-bit RGB565 source
    for (int16_t j = 0; j < height; j++) {
        for (int16_t i = 0; i < width; i++) {
            uint16_t color = data[j * width + i];
            mtb_Panel_Frame_Buffer[x + i][y + j] = color;
        }
    }

    // Send raw RGB565 pixel data to the driver (cast to byte pointer)
    mtb_Display_Driver->draw_pixels(x, y, width, height, (uint8_t *)data, Hub75PixelFormat::RGB565, Hub75ColorOrder::RGB, false);
}


// ...existing code...
void mtb_Panel_Draw_FullScreen_RGB565(uint16_t data[128][64]){
    // Build a row-major linear buffer (width x height) for the driver while
    // updating the internal frame buffer which is indexed as [x][y].
    const int16_t W = PANEL_RES_X;
    const int16_t H = PANEL_RES_Y;
    const size_t pixels = (size_t)W * (size_t)H;

    // Allocate in PSRAM if available (safe for large buffer on ESP32)
    uint16_t *linear = (uint16_t*) heap_caps_malloc(pixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!linear) {
        // As a fallback, update the backbuffer and send line by line to the driver
        for (int16_t y = 0; y < H; ++y) {
            for (int16_t x = 0; x < W; ++x) {
                uint16_t color = data[x][y];          // data is indexed [x][y]
                mtb_Panel_Frame_Buffer[x][y] = color;
                uint16_t tmp = color;
                mtb_Display_Driver->draw_pixels(x, y, 1, 1, (uint8_t*)&tmp, Hub75PixelFormat::RGB565, Hub75ColorOrder::RGB, false);
            }
        }
        return;
    }

    // Fill linear buffer row-major and update internal backbuffer
    size_t idx = 0;
    for (int16_t y = 0; y < H; ++y) {
        for (int16_t x = 0; x < W; ++x) {
            uint16_t color = data[x][y]; // original array is [128][64] => [x][y]
            mtb_Panel_Frame_Buffer[x][y] = color;
            linear[idx++] = color;
        }
    }

    // Send contiguous row-major RGB565 data to the driver
    mtb_Display_Driver->draw_pixels(0, 0, W, H, (uint8_t *)linear, Hub75PixelFormat::RGB565, Hub75ColorOrder::RGB, false);

    heap_caps_free(linear);
}
// ...existing code...


void mtb_Panel_Fill(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b){
	uint16_t color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
	for (int16_t j = 0; j < h; j++) {
		for (int16_t i = 0; i < w; i++) {
			mtb_Panel_Frame_Buffer[x + i][y + j] = color565;
		}
	}
	mtb_Display_Driver->fill(x, y, w, h, r, g, b);
}

void mtb_Panel_Set_Brightness(uint8_t brightness){
	mtb_Display_Driver->set_brightness(brightness);
}

void mtb_Panel_Fill_Screen(uint16_t color){
	// Convert RGB565 (5-6-5) to 8-bit-per-channel RGB888
	uint8_t r5 = (color >> 11) & 0x1F; // top 5 bits
	uint8_t g6 = (color >> 5) & 0x3F;  // middle 6 bits
	uint8_t b5 = color & 0x1F;         // low 5 bits

	// Expand to 8 bits. Use bit-replication to reduce banding.
	uint8_t r8 = (r5 << 3) | (r5 >> 2); // 5 -> 8 bits
	uint8_t g8 = (g6 << 2) | (g6 >> 4); // 6 -> 8 bits
	uint8_t b8 = (b5 << 3) | (b5 >> 2); // 5 -> 8 bits

	mtb_Panel_Fill(0, 0, PANEL_RES_X, PANEL_RES_Y, r8, g8, b8);
}

void mtb_Panel_Clear_Screen(void){
	// Clear backbuffer
	for (uint16_t x = 0; x < PANEL_RES_X; x++) {
		for (uint16_t y = 0; y < PANEL_RES_Y; y++) {
			mtb_Panel_Frame_Buffer[x][y] = 0; // black
		}
	}
	mtb_Display_Driver->clear();
}

void mtb_Panel_Draw_HLine(int16_t y, int16_t x1, int16_t x2, uint16_t color){
	mtb_Panel_Draw_Line(x1, y, x2, y, color);
}

void mtb_Panel_Draw_VLine(int16_t x, int16_t y1, int16_t y2, uint16_t color){
	mtb_Panel_Draw_Line(x, y1, x, y2, color);
}

void mtb_Panel_Draw_Line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color){
	// Bresenham's line algorithm (integer-only)
	int16_t dx = abs(x2 - x1);
	int16_t sx = x1 < x2 ? 1 : -1;
	int16_t dy = -abs(y2 - y1);
	int16_t sy = y1 < y2 ? 1 : -1;
	int16_t err = dx + dy; // error value e_xy

	while (true) {
		// bounds check before drawing
		if (x1 >= 0 && x1 < PANEL_RES_X && y1 >= 0 && y1 < PANEL_RES_Y) {
			mtb_Panel_Draw_Pixel565((int16_t)x1, (int16_t)y1, color);
		}

		if (x1 == x2 && y1 == y2) break;
		int16_t e2 = 2 * err;
		if (e2 >= dy) { // e_xy + e_x > 0
			err += dy;
			x1 += sx;
		}
		if (e2 <= dx) { // e_xy + e_y < 0
			err += dx;
			y1 += sy;
		}
	}
}

void mtb_Panel_Draw_Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2,  uint16_t color){
	// Normalize coordinates so x1/y1 is top-left and x2/y2 is bottom-right
	if (x2 < x1) { int16_t t = x1; x1 = x2; x2 = t; }
	if (y2 < y1) { int16_t t = y1; y1 = y2; y2 = t; }

	// Clip to display bounds (0..127, 0..63)
	if (x1 > 127) x1 = 127;
	if (x2 > 127) x2 = 127;
	if (y1 > 63) y1 = 63;
	if (y2 > 63) y2 = 63;

	// Draw four edges. Use the line routine which already bounds-checks.
	mtb_Panel_Draw_Line((int16_t)x1, (int16_t)y1, (int16_t)x2, (int16_t)y1, color); // top
	mtb_Panel_Draw_Line((int16_t)x1, (int16_t)y2, (int16_t)x2, (int16_t)y2, color); // bottom
	mtb_Panel_Draw_Line((int16_t)x1, (int16_t)y1, (int16_t)x1, (int16_t)y2, color); // left
	mtb_Panel_Draw_Line((int16_t)x2, (int16_t)y1, (int16_t)x2, (int16_t)y2, color); // right
}

void mtb_Panel_Fill_Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2,  uint16_t color){
	// Normalize coordinates so x1/y1 is top-left and x2/y2 is bottom-right
	if (x2 < x1) { int16_t t = x1; x1 = x2; x2 = t; }
	if (y2 < y1) { int16_t t = y1; y1 = y2; y2 = t; }

	// Clip to display bounds (0..127, 0..63)
	if (x1 > 127) x1 = 127;
	if (x2 > 127) x2 = 127;
	if (y1 > 63) y1 = 63;
	if (y2 > 63) y2 = 63;

	// If rectangle is empty after clipping, bail out
	if (x2 < x1 || y2 < y1) return;

	uint16_t w = x2 - x1 + 1;
	uint16_t h = y2 - y1 + 1;

	// Convert RGB565 to RGB888
	uint8_t r5 = (color >> 11) & 0x1F;
	uint8_t g6 = (color >> 5) & 0x3F;
	uint8_t b5 = color & 0x1F;
	uint8_t r8 = (r5 << 3) | (r5 >> 2);
	uint8_t g8 = (g6 << 2) | (g6 >> 4);
	uint8_t b8 = (b5 << 3) | (b5 >> 2);

	// Use the driver's optimized fill when available
	mtb_Panel_Fill(x1, y1, w, h, r8, g8, b8);
}

void mtb_Panel_Draw_Circle(int16_t x0, int16_t y0, int16_t r, uint16_t color){
	if (r < 0) return;

	int16_t x = r;
	int16_t y = 0;
	int16_t err = 1 - r; // midpoint error

	while (x >= y) {
		// plot the eight octants with bounds checks
		if (x0 + x >= 0 && x0 + x < PANEL_RES_X && y0 + y >= 0 && y0 + y < PANEL_RES_Y) mtb_Panel_Draw_Pixel565((int16_t)(x0 + x), (int16_t)(y0 + y), color);
		if (x0 - x >= 0 && x0 - x < PANEL_RES_X && y0 + y >= 0 && y0 + y < PANEL_RES_Y) mtb_Panel_Draw_Pixel565((int16_t)(x0 - x), (int16_t)(y0 + y), color);
		if (x0 + x >= 0 && x0 + x < PANEL_RES_X && y0 - y >= 0 && y0 - y < PANEL_RES_Y) mtb_Panel_Draw_Pixel565((int16_t)(x0 + x), (int16_t)(y0 - y), color);
		if (x0 - x >= 0 && x0 - x < PANEL_RES_X && y0 - y >= 0 && y0 - y < PANEL_RES_Y) mtb_Panel_Draw_Pixel565((int16_t)(x0 - x), (int16_t)(y0 - y), color);

		if (x0 + y >= 0 && x0 + y < PANEL_RES_X && y0 + x >= 0 && y0 + x < PANEL_RES_Y) mtb_Panel_Draw_Pixel565((int16_t)(x0 + y), (int16_t)(y0 + x), color);
		if (x0 - y >= 0 && x0 - y < PANEL_RES_X && y0 + x >= 0 && y0 + x < PANEL_RES_Y) mtb_Panel_Draw_Pixel565((int16_t)(x0 - y), (int16_t)(y0 + x), color);
		if (x0 + y >= 0 && x0 + y < PANEL_RES_X && y0 - x >= 0 && y0 - x < PANEL_RES_Y) mtb_Panel_Draw_Pixel565((int16_t)(x0 + y), (int16_t)(y0 - x), color);
		if (x0 - y >= 0 && x0 - y < PANEL_RES_X && y0 - x >= 0 && y0 - x < PANEL_RES_Y) mtb_Panel_Draw_Pixel565((int16_t)(x0 - y), (int16_t)(y0 - x), color);

		y++;
		if (err < 0) {
			err += 2 * y + 1;
		} else {
			x--;
			err += 2 * (y - x) + 1;
		}
	}
}

void mtb_Panel_Fill_Circle(int16_t x0, int16_t y0, int16_t r, uint16_t color){
	if (r < 0) return;

	// Convert RGB565 to RGB888 for the driver fill
	uint8_t r5 = (color >> 11) & 0x1F;
	uint8_t g6 = (color >> 5) & 0x3F;
	uint8_t b5 = color & 0x1F;
	uint8_t r8 = (r5 << 3) | (r5 >> 2);
	uint8_t g8 = (g6 << 2) | (g6 >> 4);
	uint8_t b8 = (b5 << 3) | (b5 >> 2);

	// Iterate scanlines from y0 - r to y0 + r
	int16_t y_start = y0 - r;
	int16_t y_end = y0 + r;
	if (y_start < 0) y_start = 0;
	if (y_end > 63) y_end = 63;

	const int rr = r * r;
	for (int16_t y = y_start; y <= y_end; ++y) {
		int16_t dy = y - y0;
		int32_t dx2 = rr - (int32_t)dy * (int32_t)dy;
		if (dx2 < 0) continue;
		int16_t dx = (int16_t)std::sqrt((double)dx2);

		int16_t x_left = x0 - dx;
		int16_t x_right = x0 + dx;

		if (x_right < 0 || x_left > 127) continue; // fully out of bounds
		if (x_left < 0) x_left = 0;
		if (x_right > 127) x_right = 127;

		uint16_t w = (uint16_t)(x_right - x_left + 1);
		// Use fast driver fill for the horizontal span
		mtb_Panel_Fill((uint16_t)x_left, (uint16_t)y, w, 1, r8, g8, b8);
	}
}
// Draw triangle outline by drawing three edges
void mtb_Panel_Draw_Triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color){
	mtb_Panel_Draw_Line(x0, y0, x1, y1, color);
	mtb_Panel_Draw_Line(x1, y1, x2, y2, color);
	mtb_Panel_Draw_Line(x2, y2, x0, y0, color);
}

// Fill triangle using a scanline algorithm (sort vertices by Y)
void mtb_Panel_Fill_Triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color){
	// Sort vertices by y (ascending)
	if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
	if (y1 > y2) { std::swap(y1, y2); std::swap(x1, x2); }
	if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }

	// Convert RGB565 to RGB888 once
	uint8_t r5 = (color >> 11) & 0x1F;
	uint8_t g6 = (color >> 5) & 0x3F;
	uint8_t b5 = color & 0x1F;
	uint8_t r8 = (r5 << 3) | (r5 >> 2);
	uint8_t g8 = (g6 << 2) | (g6 >> 4);
	uint8_t b8 = (b5 << 3) | (b5 >> 2);

	auto edge_interp = [](int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t y)->float{
		if (y1 == y0) return (float)x0;
		return x0 + (float)(x1 - x0) * (float)(y - y0) / (float)(y1 - y0);
	};

	// Main loop over scanlines
	int16_t y_start = y0 < 0 ? 0 : y0;
	int16_t y_end = y2 > 63 ? 63 : y2;

	for (int16_t y = y_start; y <= y_end; ++y){
		if (y < y1){
			// Interpolate between v0-v2 and v0-v1
			float xa = edge_interp(x0,y0,x2,y2,y);
			float xb = edge_interp(x0,y0,x1,y1,y);
			if (xa > xb) std::swap(xa, xb);
			int16_t x_left = (int16_t)ceilf(xa);
			int16_t x_right = (int16_t)floorf(xb);
			if (x_right < 0 || x_left > 127) continue;
			if (x_left < 0) x_left = 0;
			if (x_right > 127) x_right = 127;
			uint16_t w = x_right - x_left + 1;
			mtb_Panel_Fill((uint16_t)x_left, (uint16_t)y, w, 1, r8, g8, b8);
		} else {
			// Interpolate between v0-v2 and v1-v2
			float xa = edge_interp(x0,y0,x2,y2,y);
			float xb = edge_interp(x1,y1,x2,y2,y);
			if (xa > xb) std::swap(xa, xb);
			int16_t x_left = (int16_t)ceilf(xa);
			int16_t x_right = (int16_t)floorf(xb);
			if (x_right < 0 || x_left > 127) continue;
			if (x_left < 0) x_left = 0;
			if (x_right > 127) x_right = 127;
			uint16_t w = x_right - x_left + 1;
			mtb_Panel_Fill((uint16_t)x_left, (uint16_t)y, w, 1, r8, g8, b8);
		}
	}
}

// Draw a rounded rectangle: straight edges shortened by radius plus quarter-arc corners
void mtb_Panel_Draw_Round_Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t radius, uint16_t color){
	// Normalize coords
	if (x2 < x1) std::swap(x1, x2);
	if (y2 < y1) std::swap(y1, y2);

	// Clip radius
	int16_t w = x2 - x1 + 1;
	int16_t h = y2 - y1 + 1;
	if (radius < 0) radius = 0;
	if (radius * 2 > w) radius = w / 2;
	if (radius * 2 > h) radius = h / 2;

	// Draw straight edges (shortened by radius)
	// top
	if (x1 + radius <= x2 - radius)
		mtb_Panel_Draw_Line(x1 + radius, y1, x2 - radius, y1, color);
	// bottom
	if (x1 + radius <= x2 - radius)
		mtb_Panel_Draw_Line(x1 + radius, y2, x2 - radius, y2, color);
	// left
	if (y1 + radius <= y2 - radius)
		mtb_Panel_Draw_Line(x1, y1 + radius, x1, y2 - radius, color);
	// right
	if (y1 + radius <= y2 - radius)
		mtb_Panel_Draw_Line(x2, y1 + radius, x2, y2 - radius, color);

	if (radius == 0) return;

	// Quarter-circle drawing (midpoint) for each corner, plotting only the appropriate octants
	auto plot_quarter = [&](int16_t cx, int16_t cy, int corner){
		// corner: 0=TL,1=TR,2=BR,3=BL
		int16_t x = radius;
		int16_t y = 0;
		int16_t err = 1 - radius;
		while (x >= y) {
			// TL: plot (cx - x, cy - y) and (cx - y, cy - x)
			if (corner == 0){
				if (cx - x >= 0 && cy - y >= 0 && cx - x < PANEL_RES_X && cy - y < PANEL_RES_Y) mtb_Panel_Draw_Pixel565(cx - x, cy - y, color);
				if (cx - y >= 0 && cy - x >= 0 && cx - y < PANEL_RES_X && cy - x < PANEL_RES_Y) mtb_Panel_Draw_Pixel565(cx - y, cy - x, color);
			}
			// TR: plot (cx + x, cy - y) and (cx + y, cy - x)
			if (corner == 1){
				if (cx + x >= 0 && cy - y >= 0 && cx + x < PANEL_RES_X && cy - y < PANEL_RES_Y) mtb_Panel_Draw_Pixel565(cx + x, cy - y, color);
				if (cx + y >= 0 && cy - x >= 0 && cx + y < PANEL_RES_X && cy - x < PANEL_RES_Y) mtb_Panel_Draw_Pixel565(cx + y, cy - x, color);
			}
			// BR: plot (cx + x, cy + y) and (cx + y, cy + x)
			if (corner == 2){
				if (cx + x >= 0 && cy + y >= 0 && cx + x < PANEL_RES_X && cy + y < PANEL_RES_Y) mtb_Panel_Draw_Pixel565(cx + x, cy + y, color);
				if (cx + y >= 0 && cy + x >= 0 && cx + y < PANEL_RES_X && cy + x < PANEL_RES_Y) mtb_Panel_Draw_Pixel565(cx + y, cy + x, color);
			}
			// BL: plot (cx - x, cy + y) and (cx - y, cy + x)
			if (corner == 3){
				if (cx - x >= 0 && cy + y >= 0 && cx - x < PANEL_RES_X && cy + y < PANEL_RES_Y) mtb_Panel_Draw_Pixel565(cx - x, cy + y, color);
				if (cx - y >= 0 && cy + x >= 0 && cx - y < PANEL_RES_X && cy + x < PANEL_RES_Y) mtb_Panel_Draw_Pixel565(cx - y, cy + x, color);
			}

			y++;
			if (err < 0) {
				err += 2*y + 1;
			} else {
				x--;
				err += 2*(y - x) + 1;
			}
		}
	};

	// Corner centers
	int16_t cx_tl = x1 + radius;
	int16_t cy_tl = y1 + radius;
	int16_t cx_tr = x2 - radius;
	int16_t cy_tr = y1 + radius;
	int16_t cx_br = x2 - radius;
	int16_t cy_br = y2 - radius;
	int16_t cx_bl = x1 + radius;
	int16_t cy_bl = y2 - radius;

	plot_quarter(cx_tl, cy_tl, 0);
	plot_quarter(cx_tr, cy_tr, 1);
	plot_quarter(cx_br, cy_br, 2);
	plot_quarter(cx_bl, cy_bl, 3);
}


void mtb_Panel_Fill_Round_Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t radius, uint16_t color) {
	// Normalize coords
	if (x2 < x1) std::swap(x1, x2);
	if (y2 < y1) std::swap(y1, y2);

	// Clip radius
	int16_t w = x2 - x1 + 1;
	int16_t h = y2 - y1 + 1;
	if (radius < 0) radius = 0;
	if (radius * 2 > w) radius = w / 2;
	if (radius * 2 > h) radius = h / 2;

	// Fast path: if radius is zero, just fill rect
	if (radius == 0) {
		mtb_Panel_Fill_Rect((uint16_t)x1, (uint16_t)y1, (uint16_t)x2, (uint16_t)y2, color);
		return;
	}

	// Convert color once
	uint8_t r5 = (color >> 11) & 0x1F;
	uint8_t g6 = (color >> 5) & 0x3F;
	uint8_t b5 = color & 0x1F;
	uint8_t r8 = (r5 << 3) | (r5 >> 2);
	uint8_t g8 = (g6 << 2) | (g6 >> 4);
	uint8_t b8 = (b5 << 3) | (b5 >> 2);

	int16_t x_left_inner = x1 + radius;
	int16_t x_right_inner = x2 - radius;
	int16_t y_top_inner = y1 + radius;
	int16_t y_bottom_inner = y2 - radius;

	// 1) Fill center rectangle (if any)
	if (x_left_inner <= x_right_inner && y_top_inner <= y_bottom_inner) {
		mtb_Panel_Fill((uint16_t)x_left_inner, (uint16_t)y_top_inner,
								 (uint16_t)(x_right_inner - x_left_inner + 1), (uint16_t)(y_bottom_inner - y_top_inner + 1),
								 r8, g8, b8);
	}

	// 2) Fill top and bottom middle horizontal bars
	if (x_left_inner <= x_right_inner) {
		// top
		if (y1 <= y_top_inner - 1)
			mtb_Panel_Fill((uint16_t)x_left_inner, (uint16_t)y1, (uint16_t)(x_right_inner - x_left_inner + 1), (uint16_t)(y_top_inner - y1), r8, g8, b8);
		// bottom
		if (y_bottom_inner + 1 <= y2)
			mtb_Panel_Fill((uint16_t)x_left_inner, (uint16_t)(y_bottom_inner + 1), (uint16_t)(x_right_inner - x_left_inner + 1), (uint16_t)(y2 - y_bottom_inner), r8, g8, b8);
	}

	// 3) Fill left and right middle vertical bars
	if (y_top_inner <= y_bottom_inner) {
		// left
		if (x1 <= x_left_inner - 1)
			mtb_Panel_Fill((uint16_t)x1, (uint16_t)y_top_inner, (uint16_t)(x_left_inner - x1), (uint16_t)(y_bottom_inner - y_top_inner + 1), r8, g8, b8);
		// right
		if (x_right_inner + 1 <= x2)
			mtb_Panel_Fill((uint16_t)(x_right_inner + 1), (uint16_t)y_top_inner, (uint16_t)(x2 - x_right_inner), (uint16_t)(y_bottom_inner - y_top_inner + 1), r8, g8, b8);
	}

	// 4) Fill corner quarter-circles per scanline
	const int rr = radius * radius;

	// top corners: y = y1 .. y_top_inner - 1
	for (int16_t y = y1; y <= y_top_inner - 1; ++y) {
		int16_t dy = y - (y1 + radius);
		int32_t dx2 = rr - (int32_t)dy * (int32_t)dy;
		if (dx2 < 0) continue;
		int16_t dx = (int16_t)std::sqrt((double)dx2);

		// Top-left corner span
		int16_t cx_tl = x1 + radius;
		int16_t left_x = cx_tl - dx;
		int16_t right_x = cx_tl + dx;
		int16_t clip_left = x1;
		int16_t clip_right = x_left_inner - 1;
		if (right_x >= clip_left && left_x <= clip_right) {
			if (left_x < clip_left) left_x = clip_left;
			if (right_x > clip_right) right_x = clip_right;
			mtb_Panel_Fill((uint16_t)left_x, (uint16_t)y, (uint16_t)(right_x - left_x + 1), 1, r8, g8, b8);
		}

		// Top-right corner span
		int16_t cx_tr = x2 - radius;
		left_x = cx_tr - dx;
		right_x = cx_tr + dx;
		clip_left = x_right_inner + 1;
		clip_right = x2;
		if (right_x >= clip_left && left_x <= clip_right) {
			if (left_x < clip_left) left_x = clip_left;
			if (right_x > clip_right) right_x = clip_right;
			mtb_Panel_Fill((uint16_t)left_x, (uint16_t)y, (uint16_t)(right_x - left_x + 1), 1, r8, g8, b8);
		}
	}

	// bottom corners: y = y_bottom_inner + 1 .. y2
	for (int16_t y = y_bottom_inner + 1; y <= y2; ++y) {
		int16_t dy = y - (y_bottom_inner + radius);
		// Note: center for bottom corners is y_bottom_inner + radius == y2 - radius + radius == y2
		// But simpler: use cy = y_bottom_inner + radius == y2 - radius + radius == y2
		int32_t cy = y2 - radius + radius; // equals y2
		int16_t cy_center_top = y2 - radius; // center y for bottom corners is y2 - radius +? Wait recompute properly below
		int16_t dy_t = y - (y2 - radius);
		int32_t dx2 = rr - (int32_t)dy_t * (int32_t)dy_t;
		if (dx2 < 0) continue;
		int16_t dx = (int16_t)std::sqrt((double)dx2);

		// Bottom-left corner span
		int16_t cx_bl = x1 + radius;
		int16_t left_x = cx_bl - dx;
		int16_t right_x = cx_bl + dx;
		int16_t clip_left = x1;
		int16_t clip_right = x_left_inner - 1;
		if (right_x >= clip_left && left_x <= clip_right) {
			if (left_x < clip_left) left_x = clip_left;
			if (right_x > clip_right) right_x = clip_right;
			mtb_Panel_Fill((uint16_t)left_x, (uint16_t)y, (uint16_t)(right_x - left_x + 1), 1, r8, g8, b8);
		}

		// Bottom-right corner span
		int16_t cx_br = x2 - radius;
		left_x = cx_br - dx;
		right_x = cx_br + dx;
		clip_left = x_right_inner + 1;
		clip_right = x2;
		if (right_x >= clip_left && left_x <= clip_right) {
			if (left_x < clip_left) left_x = clip_left;
			if (right_x > clip_right) right_x = clip_right;
			mtb_Panel_Fill((uint16_t)left_x, (uint16_t)y, (uint16_t)(right_x - left_x + 1), 1, r8, g8, b8);
		}
	}
}
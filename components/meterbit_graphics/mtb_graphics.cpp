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
#include "mtb_gif_parser.h"
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#include "mtb_engine.h"
#include "LittleFS.h"
#include "mtb_github.h"
#include <HTTPClient.h>
#include <esp_heap_caps.h>  // For PSRAM allocation on ESP32
#include "nanosvg.h"
#include "nanosvgrast.h"
#include "mtb_countries.h"

// SemaphoreHandle_t pngImageDrawer_Sem = NULL;
//  Define the static queue storage variable
StaticQueue_t xQueueStorage_PNG_LocalDrawings;
// StaticQueue_t xQueueStorage_PNG_OnlineDrawings;
// StaticQueue_t xQueueStorage_SVG_OnlineDrawings;


EXT_RAM_BSS_ATTR QueueHandle_t pngLocalImageDrawer_Q = NULL;
//EXT_RAM_BSS_ATTR QueueHandle_t pngOnlineImageDrawer_Q = NULL;
//EXT_RAM_BSS_ATTR QueueHandle_t svgOnlineImageDrawer_Q = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t rgb_led_queue = NULL;
// QueueHandle_t statusBar_Q_Handle = NULL;
// SemaphoreHandle_t statusBar_Mutex_Handle = NULL;
// TaskHandle_t statusBarUpdateTask_h = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t pngLocalImageDrawer_Handle = NULL;
// EXT_RAM_BSS_ATTR TaskHandle_t pngOnlineImageDrawer_Handle = NULL;
// EXT_RAM_BSS_ATTR TaskHandle_t svgOnlineImageDrawer_Handle = NULL;

EXT_RAM_BSS_ATTR PNG_LocalImage_t statusBarItems[11];
uint16_t currentStatusLEDcolor = BLACK;
MatrixPanel_I2S_DMA *dma_display;
uint16_t panelBrightness = 70;
uint8_t **FixedText_t::scratchPad = nullptr;


EXT_RAM_BSS_ATTR Services *pngLocalImageDrawer_Sv = new Services(drawLocalPNG_Task, &pngLocalImageDrawer_Handle, "PNG Local draw", 12288, 1, pdFALSE, 1); // Keep the task stack size at 12288 for reliability.
//EXT_RAM_BSS_ATTR Services *pngOnlineImageDrawer_Sv = new Services(drawOnlinePNG_Task, &pngOnlineImageDrawer_Handle, "PNG Online draw", 12288, 4); // Keep the task stack size at 12288 for reliability.
//EXT_RAM_BSS_ATTR Services *svgOnlineImageDrawer_Sv = new Services(drawOnlineSVG_Task, &svgOnlineImageDrawer_Handle, "SVG Online draw", 12288, 4); // Keep the task stack size at 12288 for reliability.

#define RGB_LED_PIN_R 3
#define RGB_LED_PIN_G 42
#define RGB_LED_PIN_B 38

#define MAX_IMAGES 6
PNG_PreloadedImage_t preloadedPNGs[MAX_IMAGES];

#define MAX_SVG_IMAGES 6
SVG_PreloadedImage_t preloadedSVGs[MAX_SVG_IMAGES];

volatile int preloadIndex = 0;
volatile int downloadedPNGs = 0;
volatile int downloadedSVGs = 0;
SemaphoreHandle_t preloadIndexMutex = xSemaphoreCreateMutex();

void doNothingVoidFn(void){}

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

void rgb_led_task(void *param)
{
	rgb_led_message_t msg;

	while (1)
	{
		if (xQueueReceive(rgb_led_queue, &msg, portMAX_DELAY))
		{
			set_rgb_color(msg.color, msg.brightness);
			currentStatusLEDcolor = msg.color;
		}
	}
}

void set_Status_RGB_LED(uint16_t dColor, uint8_t dBrightness){
	rgb_led_message_t message = {.color = dColor, .brightness = dBrightness}; // Red color with half brightness
	xQueueSend(rgb_led_queue, &message, portMAX_DELAY);
}

void drawStatusBar(void){
	if (Applications::currentRunningApp->fullScreen == false){
		for (uint8_t i = 0; i < 11; i++) if (statusBarItems[i].xAxis != 0 && statusBarItems[i].yAxis != 0) drawLocalPNG(statusBarItems[i]);
		dma_display->drawFastHLine(0, 9, 128, dma_display->color565(35, 35, 35));
	}
}

//***PASSED ********************************************************************
void Matrix_Panel_t::config_ESP32_Panel_Pins()
{
	pinMode(ALM_BUZZER, OUTPUT); // Make the alarm silent at the very start.
	digitalWrite(ALM_BUZZER, LOW);
	if (rgb_led_queue == NULL)
		rgb_led_queue = xQueueCreate(5, sizeof(rgb_led_message_t));
	configure_ledc();
	xTaskCreatePinnedToCore(rgb_led_task, "rgb_led_task", 2048, NULL, 5, NULL, 0);
	set_Status_RGB_LED(BLACK);
}

// PASSED
void Matrix_Panel_t::init_LED_MatrixPanel(){
	uint8_t *pngLocalItems_buffer = (uint8_t *)heap_caps_malloc(20 * sizeof(PNG_LocalImage_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	//uint8_t *pngOnlineItems_buffer = (uint8_t *)heap_caps_malloc(20 * sizeof(PNG_OnlineImage_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	//uint8_t *svgOnlineItems_buffer = (uint8_t *)heap_caps_malloc(20 * sizeof(SVG_OnlineImage_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (pngLocalImageDrawer_Q == NULL) pngLocalImageDrawer_Q = xQueueCreateStatic(20, sizeof(PNG_LocalImage_t), pngLocalItems_buffer, &xQueueStorage_PNG_LocalDrawings);
	//if (pngOnlineImageDrawer_Q == NULL) pngOnlineImageDrawer_Q = xQueueCreateStatic(20, sizeof(PNG_OnlineImage_t), pngOnlineItems_buffer, &xQueueStorage_PNG_OnlineDrawings);
	//if (svgOnlineImageDrawer_Q == NULL) svgOnlineImageDrawer_Q = xQueueCreateStatic(20, sizeof(SVG_OnlineImage_t), svgOnlineItems_buffer, &xQueueStorage_SVG_OnlineDrawings);
	// Module configuration
	HUB75_I2S_CFG mxconfig(
		PANEL_RES_X, // module width
		PANEL_RES_Y, // module height
		PANEL_CHAIN	 // Chain length
	);

	// Display Setup
	dma_display = new MatrixPanel_I2S_DMA(mxconfig);
	dma_display->begin();
	read_struct_from_nvs("pan_brghnss", &panelBrightness, sizeof(uint8_t));
	dma_display->setBrightness(panelBrightness); // 0-255
	set_Status_RGB_LED(currentStatusLEDcolor, panelBrightness);
	// if(litFS_Ready)drawGIF("/mtblg/mtbStart.gif",0,0,1);
}

//************************************************
Matrix_Panel_t::Matrix_Panel_t(uint16_t x1, uint16_t y1, const uint8_t *font)
{
	setfont(font);
	x1Seg = originX1Seg = x1;
	y1Seg = originY1Seg = y1;
}

void Matrix_Panel_t::setfont(const uint8_t *font)
{
	fontMain = (uint8_t *)font;
	yAxis = font[6];
	charSpacing = font[0];
}

//*************************************************************************************
void FixedText_t::clearPanelSegment(void)
{
	for (uint16_t i = x1Seg, p = x1Seg + textHorizSpace; i < p; i++)
	{
		for (uint16_t j = y1Seg, q = y1Seg + yAxis; j < q; j++)
		{
			scratchPad[i][j] = 0;
		}
	}
}
//*************************************************************************************
void FixedText_t::updatePanelSegment(void)
{
	for (uint16_t i = x1Seg, p = x1Seg + textHorizSpace; i < p; i++)
	{
		for (uint16_t j = y1Seg, q = y1Seg + yAxis; j < q; j++)
		{
			if (!(scratchPad[i][j]))
				dma_display->drawPixel(i, j, backgroundColor); // update color
		}
	}
}
//**************************************************************************************
void Matrix_Panel_t::clearScreen(void)
{
	dma_display->clearScreen();
}
//**************************************************************************************
// PASSED
void FixedText_t::set_Pixel_Data(uint16_t xPs, uint16_t yPs)
{
	dma_display->drawPixel(xPs, yPs, color); // update color
	scratchPad[xPs][yPs] = 1;
}

//**************************************************************************************
// PASSED
void ScrollTextHelper_t::set_Pixel_Data(uint16_t xPs, uint16_t yPs)
{
	scrollBuffer[xPs][yPs] = 1;
}
//**************************************************************************************
// write character function
void Matrix_Panel_t::writeXter(uint16_t character, uint16_t xPixel, uint16_t yPixel)
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
					set_Pixel_Data(xPixel, yPixel);
				masker >>= 1;
			}
		}
	}
}
//**************************************************************************************
uint16_t Matrix_Panel_t::writeString(const char *myString)
{
	uint16_t charCount = strlen(myString);
	uint16_t row = x1Seg;
	uint16_t position = textStyle ? y1Seg : 0;

	if (textStyle)
		clearPanelSegment();

	for (uint16_t i = 0; i < charCount; row += fontMain[(myString[i] - 32) * 4 + 8] + charSpacing, ++i)
	{

		if (myString[i] > '~')
		{
			row -= fontMain[(myString[i] - 32) * 4 + 8] + charSpacing;
			continue;
		} // Filter out all Extended ASCII printable characters.

		switch (myString[i])
		{
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
			writeXter(64, row, position);
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
			continue; // Write the # symbol if the alphabet or symbol is not found.
		}
	}

	if (textStyle)
		updatePanelSegment();
	textHorizSpace = row - x1Seg;
	return row;
}
//**************************************************************************************
uint16_t Matrix_Panel_t::writeString(String myString)
{
	return writeString(myString.c_str());
}
//**************************************************************************************
uint16_t FixedText_t::writeColoredString(const char *myString, uint16_t dColor)
{
	color = dColor;
	return writeString(myString);
}
uint16_t FixedText_t::writeColoredString(String myString, uint16_t dColor)
{
	color = dColor;
	return writeString(myString);
}
//**************************************************************************************
uint16_t FixedText_t::writeColoredString(const char *myString, uint16_t dColor, uint16_t dBackgroundColor)
{
	color = dColor;
	backgroundColor = dBackgroundColor;
	return writeString(myString);
}

uint16_t FixedText_t::writeColoredString(String myString, uint16_t dColor, uint16_t dBackgroundColor)
{
	color = dColor;
	backgroundColor = dBackgroundColor;
	return writeString(myString);
}
//*************************************************************************************
void CentreText_t::clearPanelSegment(void)
{
	for (uint16_t i = x1Seg, p = x1Seg + textHorizSpace; i < p; i++)
	{
		for (uint16_t j = y1Seg, q = y1Seg + yAxis; j < q; j++)
		{
			dma_display->drawPixel(i, j, backgroundColor); // update color
		}
	}
}
//**************************************************************************************
uint16_t CentreText_t::writeColoredString(const char *myString, uint16_t dColor)
{
	color = dColor;
	clearPanelSegment();

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

	return writeString(myString);
}
//**************************************************************************************
uint16_t CentreText_t::writeColoredString(String myString, uint16_t dColor)
{
	color = dColor;
	clearPanelSegment();

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

	return writeString(myString.c_str());
}
//**************************************************************************************
uint16_t CentreText_t::writeColoredString(const char *myString, uint16_t dColor, uint16_t dBackgroundColor){
	color = dColor;
	backgroundColor = dBackgroundColor;
	clearPanelSegment();

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

	return writeString(myString);
}
//**************************************************************************************
uint16_t CentreText_t::writeColoredString(String myString, uint16_t dColor, uint16_t dBackgroundColor){
	color = dColor;
	backgroundColor = dBackgroundColor;
	clearPanelSegment();

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

	return writeString(myString.c_str());
}
//**************************************************************************************
uint16_t FixedText_t::clearString()
{
	color = backgroundColor;
	writeString(".");
	return 0;
}
//************************************************************************************ */
void drawLocalPNG_Task(void *dService){
	printf("PNG Local Image Drawer Task Started\n");
	Services *thisService = (Services *)dService;
	PNG_LocalImage_t holderItem;
	unsigned error;
	unsigned char *image = 0;
	unsigned int width, height;
	uint16_t colorHolder = 0;

	while ((xQueueReceive(pngLocalImageDrawer_Q, &holderItem, pdMS_TO_TICKS(100)) == pdTRUE) && (litFS_Ready == true)){

		// Check if the file exists, if not, download the file if internet connection exists.
		// xQueuePeek(pngLocalImageDrawer_Q, &holderItem, pdMS_TO_TICKS(100));
		if (!LittleFS.exists(holderItem.imagePath)){
			bool dwnld_Succeed = false;
			printf("PNG File Does Not Exist\n");
			if (Applications::internetConnectStatus == true){
				printf("Downloading the file......\n");
				dwnld_Succeed = downloadGithubStrgFile(String(holderItem.imagePath + 1), String(holderItem.imagePath));
			}else{
				File2Download_t dFile;
				memcpy(dFile.flashFilePath, holderItem.imagePath, 50);
				memcpy(dFile.githubFilePath, holderItem.imagePath + 1, 50);
				xQueueSend(files2Download_Q, &dFile, pdMS_TO_TICKS(1));
				printf("File Queued for later downloading.\n");
			}
			// xQueueReceive(pngLocalImageDrawer_Q, &holderItem, pdMS_TO_TICKS(100));
			if (dwnld_Succeed == false) continue; // Skip to the next item in the queue if the file does not exist.
		}

			String imageFilePath = "/littlefs" + String(holderItem.imagePath);
			error = lodepng_decode24_file(&image, &width, &height, imageFilePath.c_str());
			if (error) {
				printf("error %u: %s\n", error, lodepng_error_text(error));
				free(image);
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

							colorHolder = dma_display->color565(rAvg, gAvg, bAvg);
							dma_display->drawPixel(holderItem.xAxis + xOut, holderItem.yAxis + yOut, colorHolder);
						}
					}
				}
				free(image);
			} else {
			// printf("Image Dimensions are: Width = %d; Height = %d\n", width, height);
			free(image);
			}
	}

	kill_This_Service(thisService);
}

BaseType_t drawLocalPNG(const PNG_LocalImage_t &dImage){
	xQueueSend(pngLocalImageDrawer_Q, &dImage, 0);
	start_This_Service(pngLocalImageDrawer_Sv);
	return 0;
}

//************************************************************************************ */

// void drawOnlinePNG_Task(void *dService) {
// 	Services *thisService = (Services *)dService;
// 	PNG_OnlineImage_t holderItem;
// 	unsigned error;
// 	unsigned char *imageData = nullptr;
// 	unsigned int width, height;
// 	uint16_t colorHolder = 0;
// 	//String tempImagePath = "/tempFile/tmpImg.png";
// 	uint8_t* pngBuffer = nullptr;
// 	size_t pngSize = 0;
// 	String mimeType;


// 	while ((xQueueReceive(pngOnlineImageDrawer_Q, &holderItem, pdMS_TO_TICKS(100)) == pdTRUE) && (litFS_Ready == true)){

		
// if (downloadPNGImageToPSRAM(holderItem.imageLink, &pngBuffer, &pngSize, &mimeType)) {
//   printf("Image MIME: %s\n", mimeType.c_str());

//   // Decode with lodepng_decode24(...)

// 			// String imageFilePath = "/littlefs" + tempImagePath;
// 			// error = lodepng_decode24_file(&image, &width, &height, imageFilePath.c_str());
// 			error = lodepng_decode24(&imageData, &width, &height, pngBuffer, pngSize);

// 			if (error) {
// 				printf("error %u: %s\n", error, lodepng_error_text(error));
// 				if (imageData) free(imageData);
// 				if (pngBuffer) free(pngBuffer);
// 				continue;
// 			}

// 			// Start drawing with anti-aliased downscaling
// 			uint8_t scaleFactor = holderItem.scale;
// 			if (scaleFactor < 1) scaleFactor = 1;

// 			uint16_t scaledWidth = width / scaleFactor;
// 			uint16_t scaledHeight = height / scaleFactor;

// 			if (scaledWidth < 129 && scaledHeight < 65) {
// 				for (uint16_t yOut = 0; yOut < scaledHeight; yOut++) {
// 					for (uint16_t xOut = 0; xOut < scaledWidth; xOut++) {

// 						uint32_t rSum = 0, gSum = 0, bSum = 0;
// 						uint16_t pixelCount = 0;

// 						// Loop through the block of source pixels to average
// 						for (uint8_t dy = 0; dy < scaleFactor; dy++) {
// 							for (uint8_t dx = 0; dx < scaleFactor; dx++) {
// 								uint16_t xIn = xOut * scaleFactor + dx;
// 								uint16_t yIn = yOut * scaleFactor + dy;

// 								if (xIn >= width || yIn >= height) continue;

// 								uint32_t idx = yIn * width * 3 + xIn * 3;
// 								rSum += imageData[idx + 0];
// 								gSum += imageData[idx + 1];
// 								bSum += imageData[idx + 2];
// 								pixelCount++;
// 							}
// 						}

// 						// Compute average color
// 						if (pixelCount > 0) {
// 							uint8_t rAvg = rSum / pixelCount;
// 							uint8_t gAvg = gSum / pixelCount;
// 							uint8_t bAvg = bSum / pixelCount;

// 							colorHolder = dma_display->color565(rAvg, gAvg, bAvg);
// 							dma_display->drawPixel(holderItem.xAxis + xOut, holderItem.yAxis + yOut, colorHolder);
// 						}
// 					}
// 				}
// 				free(imageData);
// 			} else {
// 			// printf("Image Dimensions are: Width = %d; Height = %d\n", width, height);
// 			free(imageData);
// 			}
			
// 			free(pngBuffer);
// 		} else if (Applications::internetConnectStatus != true){
// 			statusBarNotif.scroll_This_Text("NO INTERNET CONNECTION TO DOWNLOAD ONLINE IMAGE.", GREEN_LIZARD);
// 		}
// 	}

// 	kill_This_Service(thisService);
// }

// BaseType_t drawOnlinePNG(const PNG_OnlineImage_t &dImage){
// 	xQueueSend(pngOnlineImageDrawer_Q, &dImage, 0);
// 	start_This_Service(pngOnlineImageDrawer_Sv);
// 	return 0;
// }

//***************************************************************************************** */
void drawDecodedPNG(PNG_PreloadedImage_t& pImg) {
    uint8_t* imageData = nullptr;
    unsigned width, height;
    uint16_t colorHolder;
    unsigned error = lodepng_decode24(&imageData, &width, &height, pImg.pngBuffer, pImg.pngSize);

    if (error || !imageData) {
        printf("LodePNG error %u: %s\n", error, lodepng_error_text(error));
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

                colorHolder = dma_display->color565(r, g, b);
                dma_display->drawPixel(pImg.meta.xAxis + xOut, pImg.meta.yAxis + yOut, colorHolder);
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

        PNG_PreloadedImage_t* img = &preloadedPNGs[myIndex];
        String mime;
        if (downloadPNGImageToPSRAM(img->meta.imageLink, &img->pngBuffer, &img->pngSize, &mime)) {
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
            free(preloadedPNGs[i].pngBuffer);
        }
    }
	//log_i("PNG Drawer Worker finished processing %d images", drawPNGsCount);
    vTaskDelete(NULL);  // ✅ Frees stack after work
}

bool drawMultiplePNGs(size_t drawPNGsCount, ImgWipeFn_ptr wipePreviousImg) {

	while(downloadedPNGs < drawPNGsCount)delay(10); // Wait for all images to be fetched from the internet
	wipePreviousImg();

	    // Start download worker tasks (e.g. 2 workers)
    for (int i = 0; i < drawPNGsCount; ++i) {
        xTaskCreatePinnedToCore(pngDrawerWorker, "IMG_DR", 4096, (void*)drawPNGsCount, 1, NULL, 1);
		delay(1); // This delay is necessary to allow the task to start ahead of the next, so more than one task doesn't an image more than once.
    }

	downloadedPNGs = 0;
	return true;
}

void downloadMultipleOnlinePNGs(const PNG_OnlineImage_t* images, size_t count) {
    if (count > MAX_IMAGES) count = MAX_IMAGES;
	preloadIndex = 0;

    // Prepare preload structures
    for (size_t i = 0; i < count; i++) {
        preloadedPNGs[i].meta = images[i];
        preloadedPNGs[i].pngBuffer = nullptr;
        preloadedPNGs[i].pngSize = 0;
        preloadedPNGs[i].isReady = false;
        preloadedPNGs[i].failed = false;
    }

    // Start download worker tasks (e.g. 2 workers)
    for (int i = 0; i < 2; ++i) {
        xTaskCreatePinnedToCore(pngDownloaderWorker, "IMG_DL", 8192, (void*)count, 1, NULL, 1);
    }
}

void drawOnlinePNGs(const PNG_OnlineImage_t* images, size_t drawPNGsCount, ImgWipeFn_ptr wipePreviousImgs){
	    downloadMultipleOnlinePNGs(images, drawPNGsCount);
        drawMultiplePNGs(drawPNGsCount, wipePreviousImgs);
}

// void drawOnlineSVG_Task(void *dService) {
//   Services *thisService = (Services *)dService;
//   PNG_OnlineImage_t holderItem;
//   uint8_t* svgBuffer = nullptr;
//   size_t svgSize = 0;
//   String mimeType;

//   while ((xQueueReceive(svgOnlineImageDrawer_Q, &holderItem, pdMS_TO_TICKS(100)) == pdTRUE) && (litFS_Ready == true)) {

//     if (downloadSVGImageToPSRAM(holderItem.imageLink, &svgBuffer, &svgSize, &mimeType)) {
//       printf("SVG MIME: %s\n", mimeType.c_str());

//       // Ensure null-terminated SVG string
//       svgBuffer[svgSize] = '\0';

//       // Parse SVG
//       NSVGimage* svg = nsvgParse((char*)svgBuffer, "px", 96.0f);
//       if (!svg) {
//         printf("Failed to parse SVG\n");
//         free(svgBuffer);
//         continue;
//       }

//       // Default dimensions if not specified
//       float svgW = svg->width > 0 ? svg->width : 1024.0f;
//       float svgH = svg->height > 0 ? svg->height : 512.0f;

//       // Maximum output size on screen
//       const float maxW = 128.0f;
//       const float maxH = 64.0f;

//       // Use user-provided scale factor (1 = full size to fit screen)
//       uint8_t userScale = holderItem.scale;
//       if (userScale < 1) userScale = 1;

//       float baseScaleX = maxW / svgW;
//       float baseScaleY = maxH / svgH;
//       float baseScale = fminf(baseScaleX, baseScaleY);

//       float scale = baseScale / userScale;

//       // Compute final render size
//       uint16_t outputW = svgW * scale;
//       uint16_t outputH = svgH * scale;

//       // Allocate pixel buffer for rasterized image (RGBA)
//       unsigned char* bitmap = (unsigned char*)ps_malloc(outputW * outputH * 4);

//       if (!bitmap) {
//         printf("Failed to allocate bitmap buffer\n");
//         nsvgDelete(svg);
//         free(svgBuffer);
//         continue;
//       }

//       NSVGrasterizer* rast = nsvgCreateRasterizer();
//       nsvgRasterize(rast, svg, 0, 0, scale, bitmap, outputW, outputH, outputW * 4);
//       nsvgDeleteRasterizer(rast);

//       // Draw pixels to screen using dma_display
//       for (uint16_t y = 0; y < outputH; y++) {
//         for (uint16_t x = 0; x < outputW; x++) {
//           uint32_t i = (y * outputW + x) * 4;
//           uint8_t r = bitmap[i + 0];
//           uint8_t g = bitmap[i + 1];
//           uint8_t b = bitmap[i + 2];
//           uint8_t a = bitmap[i + 3];

//           if (a < 128) continue; // Skip transparent pixels

//           uint16_t color = dma_display->color565(r, g, b);
//           dma_display->drawPixel(holderItem.xAxis + x, holderItem.yAxis + y, color);
//         }
//       }

//       free(bitmap);
//       nsvgDelete(svg);
//       free(svgBuffer);
//     }
//     else if (Applications::internetConnectStatus != true) {
//       statusBarNotif.scroll_This_Text("NO INTERNET CONNECTION TO DOWNLOAD SVG IMAGE.", GREEN_LIZARD);
//     }
//   }

//   kill_This_Service(thisService);
// }


// BaseType_t drawOnlineSVG(const SVG_OnlineImage_t &dImage){
// 	xQueueSend(svgOnlineImageDrawer_Q, &dImage, 0);
// 	start_This_Service(svgOnlineImageDrawer_Sv);
// 	return 0;
// }

//**************************************************************************************
void drawDecodedSVG(SVG_PreloadedImage_t& item) {
    NSVGimage* svg = nsvgParse((char*)item.svgBuffer, "px", 96.0f);
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

            if (a < 128) continue;

            uint16_t color = dma_display->color565(r, g, b);
            dma_display->drawPixel(item.meta.xAxis + x, item.meta.yAxis + y, color);
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
        SVG_PreloadedImage_t* img = &preloadedSVGs[myIndex];
        String mime;
        if (downloadSVGImageToPSRAM(img->meta.imageLink, &img->svgBuffer, &img->svgSize, &mime)) {
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
    size_t drawSVGsCount = (size_t)param;
	//log_i("SVG Drawer Worker started processing %d images", drawSVGsCount);
	// Now decode & draw
    for (int i = 0; i < drawSVGsCount; ++i){
        if (preloadedSVGs[i].isReady) {
			preloadedSVGs[i].isReady = false;
            drawDecodedSVG(preloadedSVGs[i]);
            free(preloadedSVGs[i].svgBuffer);
        }
    }
	//log_i("SVG Drawer Worker finished processing %d images", drawSVGsCount);
    vTaskDelete(NULL);  // ✅ Frees stack after work
}

bool drawMultipleSVGs(size_t drawSVGsCount, ImgWipeFn_ptr wipePreviousImg) {

	while(downloadedSVGs < drawSVGsCount)delay(10); // Wait for all images to be fetched from the internet
	wipePreviousImg();

	    // Start download worker tasks (e.g. 2 workers)
    for (int i = 0; i < drawSVGsCount; ++i) {
        xTaskCreatePinnedToCore(svgDrawerWorker, "SVG IMG_DR", 4096, (void*)drawSVGsCount, 1, NULL, 1);
		delay(1); // This delay is necessary to allow the task to start ahead of the next, so more than one task doesn't draw an image more than once.
    }

	downloadedSVGs = 0;
	return true;
}

void downloadMultipleOnlineSVGs(const SVG_OnlineImage_t* images, size_t count) {
    if (count > MAX_SVG_IMAGES) count = MAX_SVG_IMAGES;
	preloadIndex = 0;
	downloadedSVGs = 0;

    for (size_t i = 0; i < count; i++) {
        preloadedSVGs[i].meta = images[i];
        preloadedSVGs[i].svgBuffer = nullptr;
        preloadedSVGs[i].svgSize = 0;
        preloadedSVGs[i].isReady = false;
        preloadedSVGs[i].failed = false;
    }

    for (int i = 0; i < 2; ++i) {
        xTaskCreatePinnedToCore(svgDownloaderWorker, "SVG_DL", 8192, (void*)count, 3, NULL, 0);
    }
}

void drawOnlineSVGs(const SVG_OnlineImage_t* images, size_t drawSVGsCount, ImgWipeFn_ptr wipePreviousImgs){
	    downloadMultipleOnlineSVGs(images, drawSVGsCount);
        drawMultipleSVGs(drawSVGsCount, wipePreviousImgs);
}

//**************************************************************************************
void showStatusBarIcon(const PNG_LocalImage_t &pngImage)
{
	uint8_t itemIndex = pngImage.xAxis / 9;
	memcpy(&statusBarItems[itemIndex], &pngImage, sizeof(PNG_LocalImage_t));
	if (Applications::currentRunningApp->fullScreen == false)
		drawLocalPNG(pngImage);
}

void wipeStatusBarIcon(const PNG_LocalImage_t &pngImage)
{
	showStatusBarIcon({"/batIcons/wipe7x7.png", pngImage.xAxis, pngImage.yAxis});
}
//**************************************************************************************

// Function to extract flag URL
String getFlagUrlByCountryName(const String& countryName, const String& flagType) {
  const size_t CAPACITY = 150 * 1024; // ~150 KB, adjust to fit your JSON
  SpiRamJsonDocument doc(CAPACITY);

  // Parse
  auto err = deserializeJson(doc, jsonWorldCountries);
  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.f_str());
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

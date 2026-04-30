#include <stdio.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_sleep.h"
#include "nvs_flash.h"
#include "mtb_nvs.h"
#include "mtb_system.h"
#include "Arduino.h"
#include "encoder.h"
#include "mtb_engine.h"
#include "mtb_github.h"

static const char TAG[] = "mtb_system";

EXT_RAM_BSS_ATTR TaskHandle_t encoder_Task_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t button_Task_H = NULL;

EXT_RAM_BSS_ATTR StaticQueue_t xQueueStorage_EncoderEvent;
EXT_RAM_BSS_ATTR StaticQueue_t xQueueStorage_ButtonEvent;

EXT_RAM_BSS_ATTR StaticQueue_t xQueueStorage_NvsAccess;
EXT_RAM_BSS_ATTR StaticQueue_t xQueueStorage_File2Download;

EXT_RAM_BSS_ATTR StaticQueue_t xQueueStorage_AppLauncher;

EXT_RAM_BSS_ATTR Mtb_Services *mtb_Encoder_Task_Sv = new Mtb_Services(encoder_Task, &encoder_Task_H, "Encoder Task", 6144, 0, 1); // Review this task Stack Size // REVISIT -> CHECK IF THIS APP GETS CLOSED WHEN AN APP USING IT IS ENDED.
EXT_RAM_BSS_ATTR Mtb_Services *mtb_Button_Task_Sv = new Mtb_Services(button_Task, &button_Task_H, "Button Task", 6144, 0, 1); // Review this task Stack Size    // REVISIT -> CHECK IF THIS APP GETS CLOSED WHEN AN APP USING IT IS ENDED.

button_t pressButton{
  .pin = (gpio_num_t)GPIO_NUM_0,
  .poll_state_callback = NULL,
  .internal_pull = true,
  .active_low = true,
};
//Note that the variables placed in SPIRAM using EXT_RAM_BSS_ATTR will be zero initialized.
rotary_encoder_t myencoder{
  .pin_a = (gpio_num_t)GPIO_NUM_45,
  .pin_b = (gpio_num_t)GPIO_NUM_46,
  .poll_state_callback = NULL,
  .internal_pull = true,
  .active_low = true,
};

void mtb_RotaryEncoder_Init(void){
    uint8_t *encoderQueueActions_buffer = (uint8_t *)heap_caps_malloc(12 * sizeof(rotary_encoder_event_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint8_t *buttonQueueActions_buffer = (uint8_t *)heap_caps_malloc(6 * sizeof(button_event_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if(encoderEvent_Q == NULL) encoderEvent_Q = xQueueCreateStatic(12, sizeof(rotary_encoder_event_t), encoderQueueActions_buffer, &xQueueStorage_EncoderEvent); 
    if(buttonEvent_Q == NULL) buttonEvent_Q = xQueueCreateStatic(6, sizeof(button_event_t), buttonQueueActions_buffer, &xQueueStorage_ButtonEvent);   

    rotary_encoder_init(encoderEvent_Q);
    rotary_encoder_add(&myencoder);
    button_init(buttonEvent_Q);
    button_add(&pressButton);
}

void device_SD_RS(power_States_t wake_Plan){
    if(wake_Plan == TEMP_RS) esp_restart();
    mtb_Write_Nvs_Struct("wakeState",&wake_Plan, sizeof(power_States_t));
    if(wake_Plan == TEMP_SH) esp_light_sleep_start();
    else esp_deep_sleep_start();
}

void encoder_Task (void* dService){
    Mtb_Services *thisServ = (Mtb_Services *)dService;
    rotary_encoder_event_t encoder_Data;
    while (MTB_SERV_IS_ACTIVE == pdTRUE) if(xQueueReceive(encoderEvent_Q, &encoder_Data, pdMS_TO_TICKS(500)) == pdTRUE) mtb_Common_EncoderFn_ptr(encoder_Data.dir);
    mtb_Delete_This_Service(thisServ);
}

void button_Task (void* dService){
    Mtb_Services *thisServ = (Mtb_Services *)dService;
	button_event_t button_Data;
    while (MTB_SERV_IS_ACTIVE == pdTRUE) if(xQueueReceive(buttonEvent_Q, &button_Data, pdMS_TO_TICKS(500)) == pdTRUE) mtb_Common_ButtonFn_Ptr(button_Data);
    mtb_Delete_This_Service(thisServ);
}

void mtb_System_Init(void){
    uint8_t *nvsAccessQueue_buffer = (uint8_t *)heap_caps_malloc(20 * sizeof(NvsAccessParams_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint8_t *files2Download_Q_buffer = (uint8_t *)heap_caps_malloc(20 * sizeof(File2Download_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if(nvsAccessQueue == NULL) nvsAccessQueue = xQueueCreateStatic(20, sizeof(NvsAccessParams_t), nvsAccessQueue_buffer, &xQueueStorage_NvsAccess);
    if(files2Download_Q == NULL) files2Download_Q = xQueueCreateStatic(20, sizeof(File2Download_t), files2Download_Q_buffer, &xQueueStorage_File2Download);
    if(nvsAccessComplete_Sem == NULL) nvsAccessComplete_Sem = xSemaphoreCreateBinary();
    //if(bleRestoreTimer_H == NULL) bleRestoreTimer_H = xTimerCreate("bleRstoreTim", pdMS_TO_TICKS(1000), pdFALSE, NULL, bleRestoreTimerCallBkFn);
    if(Mtb_FixedText_t::scratchPad == nullptr){
            // Allocate memory for row pointers
    Mtb_FixedText_t::scratchPad = (uint8_t **)heap_caps_malloc(PANEL_RES_X * sizeof(uint8_t *), MALLOC_CAP_SPIRAM);
    if (Mtb_FixedText_t::scratchPad == NULL) {
        ESP_LOGI(TAG, "Failed to allocate memory for row pointers\n");
        return;
    }

    // Allocate memory for each row
    for (int i = 0; i < PANEL_RES_X; i++){
        Mtb_FixedText_t::scratchPad[i] = (uint8_t *)heap_caps_malloc(PANEL_RES_Y * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        if (Mtb_FixedText_t::scratchPad[i] == NULL) {
        ESP_LOGI(TAG, "Failed to allocate memory for row %d\n", i);
        return;
        }
    }
    }

    Mtb_Static_Text_t::mtb_Config_Disp_Panel_Pins();
    uint8_t *appLauncherQueue_buffer = (uint8_t *)heap_caps_malloc(6 * sizeof(Mtb_Applications*), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    init_nvs_mem();
    Mtb_Static_Text_t::mtb_Init_Led_Matrix_Panel();
	mtb_Read_Nvs_Struct("dev_Volume", &deviceVolume, sizeof(uint8_t));
    mtb_Text_Scrolls_Init();
    if(appLauncherQueue == NULL) appLauncherQueue = xQueueCreateStatic(6, sizeof(Mtb_Applications*), appLauncherQueue_buffer, &xQueueStorage_AppLauncher);

    // Read the last executed App from NVS
  

    // // Attempt OTA Update from USB Flash Drive
    mtb_Launch_This_App(usbOTA_Update_App);
    while(*(usbOTA_Update_App->appHandle_ptr) != NULL) delay(1000);

    cycling_Apps = (Mtb_Apps_To_Cycle_t){.appsShouldCycle = true, .appsNoInCycle = 5, .appCycleDuration = 15, .appsCycling = {{0,0},{0,1},{0,2},{4,0},{6,1}}, .app_Frame_Buffers = {{{0}}}}; // Default values for the app cycling struct. The app cycling feature is disabled by default, and the number of apps in cycle is set to 0. The app cycle duration is set to 10 seconds by default, and the apps in cycle are set to 0,0 (no apps) by default.

    mtb_Read_Nvs_Struct("nvsUserApp", &lastSavedApp, sizeof(Mtb_User_AppID_t));
    mtb_Read_Nvs_Struct("testCycling", &cycling_Apps, sizeof(Mtb_Apps_To_Cycle_t));
}
#include <stdio.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_sleep.h"
#include "nvs_flash.h"
#include "nvsMem.h"
#include "systemm.h"
#include "Arduino.h"
#include "encoder.h"
#include "mtbApps.h"
#include "mtbGithubStorage.h"

EXT_RAM_BSS_ATTR TaskHandle_t encoder_Task_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t button_Task_H = NULL;

EXT_RAM_BSS_ATTR Service_With_Fns *encoder_Task_Sv = new Service_With_Fns(encoder_Task, &encoder_Task_H, "Encoder Task", 10240, 0, pdTRUE, 1); // Review this task Stack Size
EXT_RAM_BSS_ATTR Service_With_Fns *button_Task_Sv = new Service_With_Fns(button_Task, &button_Task_H, "Button Task", 10240, 0, pdTRUE, 1); // Review this task Stack Size

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

void rotaryEncoder_Init(void){
    //static uint8_t fnCalledTimes = 0;
    if(encoderEvent_Q == NULL) encoderEvent_Q = xQueueCreate(12, sizeof(rotary_encoder_event_t));
    if(buttonEvent_Q == NULL) buttonEvent_Q = xQueueCreate(6, sizeof(button_event_t));
    rotary_encoder_init(encoderEvent_Q);
    rotary_encoder_add(&myencoder);
    button_init(buttonEvent_Q);
    button_add(&pressButton);
}

void device_SD_RS(power_States_t wake_Plan){
    if(wake_Plan == TEMP_RS) esp_restart();
    write_struct_to_nvs("wakeState",&wake_Plan, sizeof(power_States_t));
    if(wake_Plan == TEMP_SH) esp_light_sleep_start();
    else esp_deep_sleep_start();
}

void encoder_Task (void* dService){
    Services *thisServ = (Services *)dService;
    rotary_encoder_event_t encoder_Data;
    while (THIS_SERV_IS_ACTIVE == pdTRUE) if(xQueueReceive(encoderEvent_Q, &encoder_Data, pdMS_TO_TICKS(1)) == pdTRUE) encoderFn_ptr(encoder_Data.dir);
    kill_This_Service(thisServ);
}

void button_Task (void* dService){
    Services *thisServ = (Services *)dService;
	button_event_t button_Data;
    while (THIS_SERV_IS_ACTIVE == pdTRUE) if(xQueueReceive(buttonEvent_Q, &button_Data, pdMS_TO_TICKS(1)) == pdTRUE) buttonFn_ptr(button_Data);
    kill_This_Service(thisServ);
}

void system_Init(void){
    if(nvsAccessQueue == NULL) nvsAccessQueue = xQueueCreate(20, sizeof(NvsAccessParams_t));
    if(files2Download_Q == NULL) files2Download_Q = xQueueCreate(20, sizeof(File2Download_t));
    if(freeServAndAppPSRAM_Q == NULL) freeServAndAppPSRAM_Q = xQueueCreate(26, sizeof(void*));
    if(nvsAccessComplete_Sem == NULL) nvsAccessComplete_Sem = xSemaphoreCreateBinary();
    //if(bleRestoreTimer_H == NULL) bleRestoreTimer_H = xTimerCreate("bleRstoreTim", pdMS_TO_TICKS(1000), pdFALSE, NULL, bleRestoreTimerCallBkFn);
    if(FixedText_t::scratchPad == nullptr){
            // Allocate memory for row pointers
    FixedText_t::scratchPad = (uint8_t **)heap_caps_malloc(MATRIX_WIDTH * sizeof(uint8_t *), MALLOC_CAP_SPIRAM);
    if (FixedText_t::scratchPad == NULL) {
        //printf("Failed to allocate memory for row pointers\n");
        return;
    }

    // Allocate memory for each row
    for (int i = 0; i < MATRIX_WIDTH; i++){
        FixedText_t::scratchPad[i] = (uint8_t *)heap_caps_malloc(MATRIX_HEIGHT * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        if (FixedText_t::scratchPad[i] == NULL) {
            //printf("Failed to allocate memory for row %d\n", i);
        return;
        }
    }
    }
    Matrix_Panel_t::config_ESP32_Panel_Pins();
    init_nvs_mem();
    Matrix_Panel_t::init_LED_MatrixPanel();
	read_struct_from_nvs("dev_Volume", &deviceVolume, sizeof(uint8_t));
    text_Scrolls_Init();
    if(appLuncherQueue == NULL) appLuncherQueue = xQueueCreate(4, sizeof(Applications*));
}
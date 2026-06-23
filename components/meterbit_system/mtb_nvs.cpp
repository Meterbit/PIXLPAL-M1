/**
 * @file mtb_nvs.cpp
 * @brief NVS flash initialisation and factory-default parameter implementation.
 * Implements init_nvs_mem() (opens the default NVS namespace) and
 * set_factory_NVS_parameters() (writes firmware defaults on first boot or erase).
 */
#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>
#include <esp_netif.h>
#include "esp_system.h"
#include "esp_log.h"
#include <unistd.h>
#include "nvs.h"
#include "nvs_flash.h"

#include "mtb_graphics.h"
#include "mtb_nvs.h"
#include "mtb_system.h"
#include "mtb_text_scroll.h"
#include "mtb_ntp.h"
#include "mtb_engine.h"

static const char TAG[] = "mtb_nvs";

power_States_t wakeState = PERM_SH;
uint8_t deviceVolume = 10;
uint8_t litFS_Ready = pdTRUE;
uint8_t device_Muted = pdTRUE;

EXT_RAM_BSS_ATTR char serial_No[20];
EXT_RAM_BSS_ATTR char pxp_BLE_Name[20];
EXT_RAM_BSS_ATTR char ntp_TimeZone[100];
EXT_RAM_BSS_ATTR Clock_Colors clk_Updt;


void init_nvs_mem(void){
    esp_err_t ret = nvs_flash_init();
    //ret = ESP_ERR_NVS_NO_FREE_PAGES || ESP_ERR_NVS_NEW_VERSION_FOUND;
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
     ret = mtb_Read_Nvs_Struct("devSerial", (char*) serial_No, sizeof(serial_No));
     if(ret == ESP_OK){ 
        //set_factory_NVS_parameters();
        mtb_Read_Nvs_Struct("litFS_Ready", &litFS_Ready, sizeof(uint8_t));

        // SET DEFAULT VALUES FOR THE DEVICE PARAMETERS.
        strcpy(serial_No, "MTB000001");
        strcpy(pxp_BLE_Name, "PIXLPAL-M1");
        strcpy(ntp_TimeZone, "WAT-1");
        clk_Updt = {LASER_LEMON, LASER_LEMON, WHITE, WHITE, WHITE};
     }
     else {
        set_factory_NVS_parameters();
     }
}
 
 void set_factory_NVS_parameters(void){
    nvs_flash_erase();
    nvs_flash_init();
    mtb_Write_Nvs_Struct("pan_brghnss", &panelBrightness, sizeof(uint8_t));                      // Panel Brightness
    mtb_Write_Nvs_Struct("dev_Volume", &deviceVolume, sizeof(uint8_t));                          // Device volume
    mtb_Write_Nvs_Struct("wakeState", &wakeState, sizeof(power_States_t));                       // Device Wake State from Power down
    mtb_Write_Nvs_Struct("litFS_Ready", &litFS_Ready, sizeof(uint8_t));                          // Spiff health status (Ready or Corrupt)
    mtb_Write_Nvs_Struct("Device_muted", &device_Muted, sizeof(uint8_t));                        // Device Muted or Unmuted


    // SET DEFAULT VALUES FOR THE DEVICE PARAMETERS.
    strcpy(serial_No, "MTB000001");
    strcpy(pxp_BLE_Name, "PIXLPAL-M1");
    strcpy(ntp_TimeZone, "WAT-1");
    clk_Updt = {LASER_LEMON, LASER_LEMON, WHITE, WHITE, WHITE};
 }

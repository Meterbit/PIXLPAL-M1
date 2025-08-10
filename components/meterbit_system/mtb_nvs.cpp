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
#include "mtb_gif_parser.h"
#include "mtb_engine.h"


static const char TAG[] = "mtb_nvs";
power_States_t wakeState = PERM_SH;
uint8_t deviceVolume = 10;
uint16_t anime_Clock_Intv = 60000;
uint8_t litFS_Ready = pdTRUE;
uint8_t device_Muted = pdTRUE;
uint8_t device_App = pdTRUE;        //Remove this please, it was used to take the update to 1.0.0

char serial_No[20] = "MTB-F1-000001";
char pxp_BLE_Name[20] = "PIXLPAL-M1";
char ntp_TimeZone[100] = "WAT-1";

// Mtb_Applications DEFAULT SETTINGS *******************************************************************************************

Clock_Colors clk_Updt{
    .hourMinColour = LASER_LEMON,
    .secColor = LASER_LEMON,
    .meridiemColor = WHITE,
    .weekDayColour = WHITE,
    .dateColour = WHITE,
};

//**************************************************************************************************************************
Wifi_Credentials default_Successful_Wifi{
    "MTB_TEST",
    "password",
    "192.168.0.1"
    // "connected",
};

CurrentApp_t classickClockAppSelect{
    .GenApp = 0,
    .SpeApp = 0
    };

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
     }
     else {
        set_factory_NVS_parameters();
     }
}
 
 void set_factory_NVS_parameters(void){
    nvs_flash_erase();
    nvs_flash_init();
    mtb_Write_Nvs_Struct("devSerial", (char*) serial_No, sizeof(serial_No));                     // Serial No
    mtb_Write_Nvs_Struct("pxpBleDevName", (char*) pxp_BLE_Name, sizeof(pxp_BLE_Name));           // pxp BLE Name
    mtb_Write_Nvs_Struct("ntp TimeZone", (char*) ntp_TimeZone, sizeof(ntp_TimeZone));            // NTP Time-Zone
    mtb_Write_Nvs_Struct("pan_brghnss", &panelBrightness, sizeof(uint8_t));                      // Panel Brightness
    mtb_Write_Nvs_Struct("dev_Volume", &deviceVolume, sizeof(uint8_t));                          // Device volume
    mtb_Write_Nvs_Struct("currentApp", &classickClockAppSelect, sizeof(CurrentApp_t));           // Current Running App
    mtb_Write_Nvs_Struct("Clock Cols", &clk_Updt, sizeof(Clock_Colors));                         // Clock Colors
    mtb_Write_Nvs_Struct("Wifi Cred",&default_Successful_Wifi, sizeof(Wifi_Credentials));        // Successful Wi-Fi Credentials
    mtb_Write_Nvs_Struct("wakeState", &wakeState, sizeof(power_States_t));                       // Device Wake State from Power down
    mtb_Write_Nvs_Struct("litFS_Ready", &litFS_Ready, sizeof(uint8_t));                          // Spiff health status (Ready or Corrupt)
    mtb_Write_Nvs_Struct("Device_muted", &device_Muted, sizeof(uint8_t));                        // Device Muted or Unmuted
 }

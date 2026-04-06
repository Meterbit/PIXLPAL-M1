
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "lwip/api.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "string.h"
#include "cJSON.h"
#include "nvs.h"
#include <esp_app_desc.h>
#include <esp_ghota.h>
#include "Arduino.h"
#include "ArduinoJson.h"
#include "mtb_nvs.h"
#include "mtb_wifi.h"
#include "mtb_ble.h"
#include "NimBLEDevice.h"
#include "mtb_ota.h"
#include "mtb_ble_ota.h"

static const char TAG[] = "MTB-BLE-UPDATE";
EXT_RAM_BSS_ATTR TaskHandle_t bleFirmwareUpdate_H = NULL;
void bleFirmwareUpdateTask(void* dApplication);
void stopButton_BLE_OTA_UPDATE (button_event_t button_Data);
void attemptSoftwareUpdate(JsonDocument&);
void ble_Ota_Control(JsonDocument &doc);
void abort_Ble_Ota_Update_Manually(void);


EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *bleOTA_Update_App = new Mtb_Applications_FullScreen(bleFirmwareUpdateTask, &bleFirmwareUpdate_H, "BLE OTA TASK", 6144);

void  bleFirmwareUpdateTask(void* dApplication){
    Mtb_Applications *THIS_APP = (Mtb_Applications *) dApplication;
    THIS_APP->mtb_App_Set_EC11_Cb_Fns(stopButton_BLE_OTA_UPDATE);
    THIS_APP->mtb_App_Init();

    Mtb_CentreText_t bleOTA_Update_Text(64, 50, Terminal8x12);

    uint16_t countdown = 0;

    if(litFS_Ready){
      Mtb_LocalImage_t otaBLE_Image {"/batIcons/bleOTA.png", 0, 0};
      mtb_Draw_Local_Png(otaBLE_Image);
    }

    while (THIS_APP_IS_ACTIVE == pdTRUE){
      countdown = 1500;
      bleOTA_Update_Text.mtb_Write_Colored_Text("BLUETOOTH OTA UPDATE IN PROGRESS....", GREEN);
      bleOTA_Update_Text.mtb_Write_Colored_Text("DO NOT SWITCH OFF DEVICE", YELLOW);
      while(countdown-->0  && THIS_APP_IS_ACTIVE) delay(10);
    }

    mtb_Delete_This_App(THIS_APP);
  }


void softwareUpdate(JsonDocument& dCommand) {
    DeserializationError passed;
    uint16_t dCmd_num = 0;

    dCmd_num = dCommand["set_command"];

    switch (dCmd_num) {
        case 1:{
            const esp_app_desc_t *app_desc = esp_app_get_description();

            mtb_Current_Software_Version(app_desc->version, app_desc->date, WiFi.macAddress().c_str(), NimBLEDevice::getAddress().toString().c_str());
            break;
        }
        case 2:
            attemptSoftwareUpdate(dCommand);
        break;

        case 3:
            ble_Ota_Control(dCommand);
        break;

        default:
            ESP_LOGI(TAG, "pxpBLE Settings Number not Recognised.\n");
            break;
    }
}

//**01*********************************************************************************************************************
void mtb_Current_Software_Version(const char* curVer, const char* verDate, const char* wifiMac, const char* bleMac) {
String verHeader = "{\"set_command\": 1, \"curVersion\": \"";
String currentVersion = verHeader + String(curVer) + "\", \"curDate\": \"" + String(verDate) + "\", \"wifiMac\": \"" + String(wifiMac) + "\", \"bleMac\": \"" + String(bleMac) + "\"}";
//ESP_LOGI(TAG, "Current Software Version: %s\n", currentVersion.c_str());
bleSettingsComSend(mtb_Software_Update_Route, currentVersion);
}

// //**02*********************************************************************************************************************
void attemptSoftwareUpdate(JsonDocument& dCommand){ 

String failure = "{\"set_command\": 2, \"response\": 0}";

if(Mtb_Applications::internetConnectStatus == pdTRUE){
  mtb_Launch_This_App(ghotaOTA_Update_App, IGNORE_PREVIOUS_APP);
} else{
  bleSettingsComSend(mtb_Software_Update_Route, failure);
}
}


void ble_Ota_Control(JsonDocument &doc)
{
    const char *action = doc["action"] | "";

    if (strcmp(action, "enter_ble_ota") == 0) {
        mtb_Ble_Ota_Enter_Ready_Mode();
        mtb_Launch_This_App(bleOTA_Update_App);
        bleSettingsComSend(mtb_Software_Update_Route, "{\"set_command\": 3,\"response\":1,\"mode\":\"ble_ota_ready\"}");
        return;
    }

    if (strcmp(action, "cancel_ble_ota") == 0) {
        mtb_Ble_Ota_Set_Enabled(false);
        mtb_Ble_Ota_Abort();
        mtb_Launch_This_App(Mtb_Applications::previousRunningApp);
        bleSettingsComSend(mtb_Software_Update_Route, "{\"set_command\": 3,\"response\":1,\"mode\":\"ble_ota_canceled\"}");
        return;
    }

    bleSettingsComSend(mtb_Software_Update_Route, "{\"set_command\": 3,\"response\":0}");
}

void stopButton_BLE_OTA_UPDATE (button_event_t button_Data){
            switch (button_Data.type){
            case BUTTON_RELEASED:
            break;

            case BUTTON_PRESSED:
            abort_Ble_Ota_Update_Manually();
            break;

            case BUTTON_PRESSED_LONG:
            break;

            case BUTTON_CLICKED:
            //ESP_LOGI(TAG, "Button Clicked: %d Times\n",button_Data.count);
            switch (button_Data.count){
            case 1:
                break;
            case 2:
                break;
            case 3:
                break;
            default:
                break;
            }
                break;
            default:
            break;
			}
}

void abort_Ble_Ota_Update_Manually(void){
    mtb_Ble_Ota_Set_Enabled(false);
    mtb_Ble_Ota_Abort();
    mtb_Launch_This_App(Mtb_Applications::previousRunningApp);
    bleSettingsComSend(mtb_Software_Update_Route, "{\"set_command\": 3,\"response\":1,\"mode\":\"ble_ota_canceled\"}");
}
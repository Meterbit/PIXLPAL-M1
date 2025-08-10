
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

static const char TAG[] = "MTB-FLUTTERBLE-GHOTA";

void attemptSoftwareUpdate(DynamicJsonDocument&);

void softwareUpdate(DynamicJsonDocument& dCommand) {
    DeserializationError passed;
    uint16_t dCmd_num = 0;

    dCmd_num = dCommand["set_command"];

    switch (dCmd_num) {
        case 1:{
            const esp_app_desc_t *app_desc = esp_app_get_description();

            current_softwareVersion(app_desc->version, app_desc->date, WiFi.macAddress().c_str(), NimBLEDevice::getAddress().toString().c_str());
            break;
        }
        case 2:
            attemptSoftwareUpdate(dCommand);
            break;

        default:
            printf("pxpBLE Settings Number not Recognised.\n");
            break;
    }
}

//**01*********************************************************************************************************************
void current_softwareVersion(const char* curVer, const char* verDate, const char* wifiMac, const char* bleMac) {
String verHeader = "{\"pxp_command\": 1, \"curVersion\": \"";
String currentVersion = verHeader + String(curVer) + "\", \"curDate\": \"" + String(verDate) + "\", \"wifiMac\": \"" + String(wifiMac) + "\", \"bleMac\": \"" + String(bleMac) + "\"}";
//printf("Current Software Version: %s\n", currentVersion.c_str());
bleSettingsComSend(softwareUpdateRoute, currentVersion);
}

// //**02*********************************************************************************************************************
void attemptSoftwareUpdate(DynamicJsonDocument& dCommand){ 

String failure = "{\"pxp_command\": 2, \"response\": 0}";

if(Applications::internetConnectStatus == pdTRUE){
  launchThisApp(otaUpdateApplication_App, IGNORE_PREVIOUS_APP);
} else{
  bleSettingsComSend(softwareUpdateRoute, failure);
}
}
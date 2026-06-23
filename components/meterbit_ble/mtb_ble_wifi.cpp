/**
 * @file mtb_ble_wifi.cpp
 * @brief BLE Wi-Fi settings command handler (query / connect to network).
 *
 * Implements wifiSettings() (dispatch), current_Network() (reports connected SSID and
 * IP to the companion app), and connect_To_Network() (attempts a new Wi-Fi connection
 * with credentials supplied via BLE, with a 10-second timeout).
 */
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

#include "Arduino.h"
#include "ArduinoJson.h"
#include "mtb_nvs.h"
#include "mtb_wifi.h"
#include "mtb_ble.h"

static const char TAG[] = "BLE_WIFI_SETTINGS";

void connect_To_Network(JsonDocument&);

void wifiSettings(JsonDocument& dCommand){
    DeserializationError passed;
    uint16_t dCmd_num = 0;

    dCmd_num = dCommand["set_command"];

    switch(dCmd_num){
    case 1: wifi_CurrentContdNetwork();
      break;
    case 2: connect_To_Network(dCommand);
      break;
    default: ESP_LOGI(TAG, "Wi-fi Settings Number not Recognised.\n");
      break;
    }
}

//**01*********************************************************************************************************************
void current_Network(const char* networkName, const char* assigned_IP){
char sta_Contd[150] = "{\"set_command\": 1, \"connected\": 1, \"nwkName\": \"";
String sta_Nt_Contd = "{\"set_command\": 1, \"connected\": 0}";

if (networkName == NULL) bleSettingsComSend(mtb_Wifi_Settings_Route, sta_Nt_Contd);
else if(strlen(networkName) > 0){
  strcat(sta_Contd, networkName);
  strcat(sta_Contd, "\",\"ipAddress\":\"");
  strcat(sta_Contd, assigned_IP);
  strcat(sta_Contd, "\"}");
  bleSettingsComSend(mtb_Wifi_Settings_Route, String(sta_Contd));
}
}

//**03*********************************************************************************************************************
void connect_To_Network(JsonDocument& dCommand){
unsigned long startTime = millis();
unsigned long timeout = 10000; // 10 seconds timeout
char sta_Contd[150] = "{\"set_command\": 1, \"connected\": 1}";
String sta_Nt_Contd = "{\"set_command\": 1, \"connected\": 0}";
String ssid = dCommand["nwkName"];
String  password = dCommand["nwkPass"];
strcpy(last_Successful_Wifi.ssid, ssid.c_str());
strcpy(last_Successful_Wifi.pass, password.c_str());
WiFi.disconnect(true, true);
delay(200);  // allow state machine to reset
WiFi.begin(ssid, password);
while ((millis() - startTime) < timeout) delay(500);
if(WiFi.status() != WL_CONNECTED) bleSettingsComSend(mtb_Wifi_Settings_Route, sta_Nt_Contd);
}

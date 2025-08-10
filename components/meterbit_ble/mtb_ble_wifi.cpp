
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
    default: printf("Wi-fi Settings Number not Recognised.\n");
      break;
    }
}

//**01*********************************************************************************************************************
void current_Network(const char* networkName, const char* assigned_IP){
//const char dTopic[] = "Pixlpal/Config/2";
char sta_Contd[150] = "{\"pxp_command\": 1, \"connected\": 1, \"nwkName\": \"";
String sta_Nt_Contd = "{\"pxp_command\": 1, \"connected\": 0}";

if (networkName == NULL) bleSettingsComSend("2", sta_Nt_Contd);
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
char sta_Contd[150] = "{\"pxp_command\": 1, \"connected\": 1}";
String sta_Nt_Contd = "{\"pxp_command\": 1, \"connected\": 0}";
String ssid = dCommand["nwkName"];
String  password = dCommand["nwkPass"];
strcpy(last_Successful_Wifi.ssid, ssid.c_str());
strcpy(last_Successful_Wifi.pass, password.c_str());
WiFi.begin(ssid, password);
while ((millis() - startTime) < timeout) delay(500);
if(WiFi.status() != WL_CONNECTED) bleSettingsComSend(mtb_Wifi_Settings_Route, sta_Nt_Contd);
}

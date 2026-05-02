
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
#include "mtb_ble.h"

static const char TAG[] = "BLE_WIFI_SETTINGS";

void appCycling_Status(JsonDocument&);
void appCycling_Duration(JsonDocument&);
void appCycling_Selections(JsonDocument&);
void getUserCyclingApps(JsonDocument&);

void app_Cycle_Settings(JsonDocument& dCommand){
    DeserializationError passed;
    uint16_t dCmd_num = 0;

    dCmd_num = dCommand["set_command"];

    switch(dCmd_num){
    case 1: appCycling_Status(dCommand);
      break;
    case 2: appCycling_Duration(dCommand);
      break;
    case 3: appCycling_Selections(dCommand);
      break;
    case 4: getUserCyclingApps(dCommand);
      break;
    default: ESP_LOGW(TAG, "App Cycling Settings Number not Recognised.\n");
      break;
    }

    mtb_Write_Nvs_Struct("testCycling", &cycling_Apps, sizeof(Mtb_Apps_To_Cycle_t));
}

//**01*********************************************************************************************************************
void appCycling_Status(JsonDocument& dCommand){
  bool tempStatus;
  char* acknowledge = "{\"set_command\": 1, \"response\": 1}";
  tempStatus = dCommand["value"];
  cycling_Apps.appsShouldCycle = tempStatus;

  if(cycling_Apps.appsShouldCycle && cycling_Apps.appsNoInCycle > 0){
    mtb_Kill_This_App(Mtb_Applications::currentRunningApp);
    mtb_Launch_This_Service(mtb_App_Cycling_Sv);
  } else mtb_Kill_This_Service(mtb_App_Cycling_Sv);

  bleSettingsComSend(mtb_App_Cycle_Settings_Route, acknowledge);
}

//**01*********************************************************************************************************************
void appCycling_Duration(JsonDocument& dCommand){
uint16_t tempDuration;
String acknowledge = "{\"set_command\": 2, \"response\": 1}";
tempDuration = dCommand["value"];
cycling_Apps.appCycleDuration = tempDuration;
bleSettingsComSend(mtb_App_Cycle_Settings_Route, acknowledge);
}

//**03*********************************************************************************************************************
void appCycling_Selections(JsonDocument& dCommand){
  printf("appCycling_Selections command received.\n");
  Mtb_User_AppID_t userAppHolder;
  String jsonString;
  JsonDocument doc;
  JsonArray apps;

  bool dAction = dCommand["action"];
  String appID = dCommand["appID"];

  userAppHolder.GenApp = getIntegerAtIndex(appID, 0);
  userAppHolder.SpeApp = getIntegerAtIndex(appID, 1);

  if(dAction){
  mtb_Add_App_To_Cycle_App_List(userAppHolder);
  doc["app_command"] = 250;
  apps = doc["apps"].to<JsonArray>();

  for(uint8_t i = 0; i < cycling_Apps.appsNoInCycle; i++){
    String appEntry = String(cycling_Apps.appsCycling[i].GenApp) + "/" + String(cycling_Apps.appsCycling[i].SpeApp);
    apps.add(appEntry);
  }

  serializeJson(doc, jsonString);
  bleApplicationComSend(appID.c_str(), jsonString.c_str());
  }else{
  mtb_Remove_App_From_Cycle_App_List(userAppHolder);

  doc["app_command"] = 251;
  apps = doc["apps"].to<JsonArray>();

  for(uint8_t i = 0; i < cycling_Apps.appsNoInCycle; i++){
    String appEntry = String(cycling_Apps.appsCycling[i].GenApp) + "/" + String(cycling_Apps.appsCycling[i].SpeApp);
    apps.add(appEntry);
  }

  serializeJson(doc, jsonString);
  bleApplicationComSend(appID.c_str(), jsonString.c_str());

  }
}

//**04*********************************************************************************************************************
void getUserCyclingApps(JsonDocument& dCommand){
  printf("getUserCyclingApps command received.\n");

  String jsonString;
  JsonDocument doc;
  doc["set_command"] = 4;
  JsonArray apps = doc["apps"].to<JsonArray>();

  for(uint8_t i = 0; i < CYCLING_APP_SLOTS; i++){
    String appEntry = String(cycling_Apps.appsCycling[i].GenApp) + "/" + String(cycling_Apps.appsCycling[i].SpeApp);
    apps.add(appEntry);
  }

  serializeJson(doc, jsonString);
  bleSettingsComSend(mtb_App_Cycle_Settings_Route, jsonString.c_str());
  //printf("Sending user cycling apps data: %s\n", jsonString.c_str());
}
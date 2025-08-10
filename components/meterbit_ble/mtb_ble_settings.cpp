
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

void bleSettings(JsonDocument& dCommand){
    DeserializationError passed;
    uint16_t dCmd_num = 0;

    dCmd_num = dCommand["set_command"];

    switch(dCmd_num){
    case 1: 
    read_struct_from_nvs("pxpBleDevName", pxp_BLE_Name, sizeof(pxp_BLE_Name));
    mtb_Current_Ble_Device(pxp_BLE_Name);
      break;
    case 2:
    mtb_Set_Pxp_Ble_Name(dCommand);
      break;
    default: printf("pxpBLE Settings Number not Recognised.\n");
      break;
    }
}

//**01*********************************************************************************************************************
void mtb_Current_Ble_Device(const char* pxpBleName){
char ble_Contd[150] = "{\"pxp_command\": 1, \"connected\": 1, \"pxpBleName\": \"";
String ble_Nt_Contd = "{\"pxp_command\": 1, \"connected\": 0}";

if (pxpBleName == NULL) bleSettingsComSend(mtb_Ble_Settings_Route, ble_Nt_Contd);
else if(strlen(pxpBleName) > 0){
  strcat(ble_Contd, pxpBleName);
  strcat(ble_Contd, "\"}");
  bleSettingsComSend(mtb_Ble_Settings_Route, String(ble_Contd));
}
}

//**02*********************************************************************************************************************
void mtb_Set_Pxp_Ble_Name(JsonDocument& dCommand){ 
String success = "{\"pxp_command\": 2, \"response\": 1}";
String emptyName = "{\"pxp_command\": 2, \"response\": 0}";
String newBleName = dCommand["newBleName"];
if (newBleName.length() == 0) {
    bleSettingsComSend(mtb_Ble_Settings_Route, emptyName);
    return;
}
strcpy(pxp_BLE_Name, newBleName.c_str());
write_struct_to_nvs("pxpBleDevName", pxp_BLE_Name, sizeof(pxp_BLE_Name));
bleSettingsComSend(mtb_Ble_Settings_Route, success);
}
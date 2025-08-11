
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
#include "mtb_system.h"
#include "mtb_ble.h"
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#include "mtb_graphics.h"

static const char TAG[] = "BLE_SYSTEM_SETTINGS";

void system_Clock_Format_Change(JsonDocument&);
void system_Device_Brightness(JsonDocument&);
void system_Silent_Mode(JsonDocument&);
void system_PowerSaver_Mode(JsonDocument&);
void system_Wifi_Radio(JsonDocument&);
void system_Time_Zone(JsonDocument&);
void system_Restart_Device();
void system_Shutdown_Device();

void systemSettings(JsonDocument& dCommand){
    DeserializationError passed;
    uint16_t dCmd_num = 0;

    dCmd_num = dCommand["set_command"];

    switch(dCmd_num){
    case 1: system_Device_Brightness(dCommand);
        break;
    case 2: system_Silent_Mode(dCommand);
        break;
    case 3: system_PowerSaver_Mode(dCommand);
        break;
    case 4: system_Wifi_Radio(dCommand);
        break;
    case 5: system_Time_Zone(dCommand);
        break;
    case 6: system_Clock_Format_Change(dCommand);
        break;
    case 7: system_Restart_Device();
        break;
    case 8: system_Shutdown_Device();
        break;
    // case 9: power_Saver_Mode(dCommand);
    //     break;
    // case 10: power_Wifi_Radio(dCommand);
    //     break;
    // case 11: power_Restart_Device();
    //     break;
    // case 12: power_Shutdown_Device();
    //     break;
    default: ESP_LOGI(TAG, "System Settings Number not Recognised.\n");
      break;
    }
}

//**01*********************************************************************************************************************
void system_Device_Brightness(JsonDocument& dCommand){    // In the App, prevent multiple values from being sent by waiting for response after the first value has been sent.
int tempBrightness;
char setPanBrightness[100] = "{\"pxp_command\": 1, \"value\": ";
char brightnsValue[10];

tempBrightness = dCommand["value"];

panelBrightness = (tempBrightness * 2.55) + 1; // One (1) is added to make the 100% correspond to 255
if (panelBrightness == 0)panelBrightness = 5;
mtb_Write_Nvs_Struct("pan_brghnss", &panelBrightness, sizeof(uint8_t));
dma_display->setBrightness(panelBrightness); // 0-255
mtb_Set_Status_RGB_LED(currentStatusLEDcolor);
sprintf(brightnsValue, "%d", (uint8_t)(panelBrightness / 2.55));
strcat(setPanBrightness, brightnsValue);
strcat(setPanBrightness, "}");
bleSettingsComSend(mtb_System_Settings_Route, String(setPanBrightness));
}

//**02*********************************************************************************************************************
void system_Silent_Mode(JsonDocument& dCommand){
  String success = "{\"pxp_command\": 2, \"response\": 1}";
  bleSettingsComSend(mtb_System_Settings_Route, success);
}
//**03*********************************************************************************************************************
void system_PowerSaver_Mode(JsonDocument& dCommand){
  String success = "{\"pxp_command\": 3, \"response\": 1}";
  bleSettingsComSend(mtb_System_Settings_Route, success);
}
//**04*********************************************************************************************************************
void system_Wifi_Radio(JsonDocument& dCommand){
  String success = "{\"pxp_command\": 4, \"response\": 1}";
  bleSettingsComSend(mtb_System_Settings_Route, success);
}
//**05*********************************************************************************************************************
void system_Time_Zone(JsonDocument& dCommand){
  String success = "{\"pxp_command\": 5, \"response\": 1}";
  bleSettingsComSend(mtb_System_Settings_Route, success);
}
//**06*********************************************************************************************************************
void system_Clock_Format_Change(JsonDocument& dCommand){
    String success = "{\"pxp_command\": 6, \"response\": 1}";
    //uint8_t clockType = dCommand["value"];
    // if(clockType == 0){// Start the Classic Clock
    //   nvs_set_u8(my_nvs_handle, "clock_Face", 1);
    //   nvs_commit(my_nvs_handle);
    //   //vTaskSuspend(clockGifHandle);
    // }   
    //else;   // Set the Clock Type
    bleSettingsComSend(mtb_System_Settings_Route, success);
}

//**07*********************************************************************************************************************
void system_Restart_Device(){
  statusBarNotif.mtb_Scroll_This_Text("RESTARTING PIXLPAL", YELLOW);
  String acknowledge = "{\"pxp_command\": 7, \"response:\": 1}";
  bleSettingsComSend(mtb_System_Settings_Route, acknowledge);
  delay(10000);
  device_SD_RS(TEMP_RS);
}
//**08*********************************************************************************************************************
void system_Shutdown_Device(){
  statusBarNotif.mtb_Scroll_This_Text("SHUTTING DOWN PIXLPAL", RED);
  String acknowledge = "{\"pxp_command\": 8, \"response:\": 1}";
  delay(12000);
  bleSettingsComSend(mtb_System_Settings_Route, acknowledge);
	device_SD_RS(PERM_SH);
  mtb_Set_Status_RGB_LED(RED);
}
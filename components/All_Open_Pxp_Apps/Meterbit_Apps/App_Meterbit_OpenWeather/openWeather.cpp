#include <Arduino.h>
#include <HTTPClient.h>
#include "mtb_github.h"
#include "mtb_text_scroll.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "mtb_nvs.h"
#include "openWeather.h"
#include "mtb_engine.h"

OpenWeatherData_t currentOpenWeatherData = {
    "Lagos, Nigeria"
};

EXT_RAM_BSS_ATTR TaskHandle_t openWeather_Task_H = NULL;
void openWeather_App_Task(void *);
// supporting functions

// button and encoder functions
void changeOpenWeatherLocation(button_event_t button_Data);

// bluetooth functions
void setOpenWeatherLocation(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *openWeather_App = new Mtb_Applications_StatusBar(openWeather_App_Task, &openWeather_Task_H, "Open Weather", 10240);

void openWeather_App_Task(void* dApplication){
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_EncoderFn_ptr = mtb_Brightness_Control;
  thisApp->mtb_App_ButtonFn_ptr = changeOpenWeatherLocation;
  mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(setOpenWeatherLocation);
  mtb_App_Init(thisApp, mtb_Status_Bar_Clock_Sv);
  //************************************************************************************ */
  mtb_Read_Nvs_Struct("openWeather", &currentOpenWeatherData, sizeof(OpenWeatherData_t));

while (MTB_APP_IS_ACTIVE == pdTRUE) {

    while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (MTB_APP_IS_ACTIVE == pdTRUE) {}

}

  mtb_End_This_App(thisApp);
}

void changeOpenWeatherLocation(button_event_t button_Data){
            switch (button_Data.type){
            case BUTTON_RELEASED:
            break;

            case BUTTON_PRESSED:
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

void setOpenWeatherLocation(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["location"];
    mtb_Write_Nvs_Struct("openWeather", &currentOpenWeatherData, sizeof(OpenWeatherData_t));
    mtb_Ble_App_Cmd_Respond_Success(openWeatherAppRoute, cmdNumber, pdPASS);
}
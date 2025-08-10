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
void setOpenWeatherLocation(DynamicJsonDocument&);

EXT_RAM_BSS_ATTR Applications_StatusBar *openWeather_App = new Applications_StatusBar(openWeather_App_Task, &openWeather_Task_H, "Open Weather", 10240);

void openWeather_App_Task(void* dApplication){
  Applications *thisApp = (Applications *)dApplication;
  thisApp->app_EncoderFn_ptr = brightnessControl;
  thisApp->app_ButtonFn_ptr = changeOpenWeatherLocation;
  ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(setOpenWeatherLocation);
  appsInitialization(thisApp, statusBarClock_Sv);
  //************************************************************************************ */
  read_struct_from_nvs("openWeather", &currentOpenWeatherData, sizeof(OpenWeatherData_t));

while (THIS_APP_IS_ACTIVE == pdTRUE) {

    while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (THIS_APP_IS_ACTIVE == pdTRUE) {}

}

  kill_This_App(thisApp);
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
            //printf("Button Clicked: %d Times\n",button_Data.count);
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

void setOpenWeatherLocation(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["location"];
    write_struct_to_nvs("openWeather", &currentOpenWeatherData, sizeof(OpenWeatherData_t));
    ble_Application_Command_Respond_Success(openWeatherAppRoute, cmdNumber, pdPASS);
}
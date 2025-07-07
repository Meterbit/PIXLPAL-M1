#include <Arduino.h>
#include <HTTPClient.h>
#include "mtbGithubStorage.h"
#include "scrollMsgs.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "nvsMem.h"
#include "googleWeather.h"
#include "mtbApps.h"

GoogleWeatherData_t currentGoogleWeatherData = {
    "Lagos, Nigeria"
};

EXT_RAM_BSS_ATTR TaskHandle_t googleWeather_Task_H = NULL;
void googleWeatherUpdate_App_Task(void *);
// supporting functions

// button and encoder functions
void changeGoogleWeatherLocation(button_event_t button_Data);

// bluetooth functions
void setGoogleWeatherLocation(DynamicJsonDocument&);

EXT_RAM_BSS_ATTR Applications_StatusBar *googleWeather_App = new Applications_StatusBar(googleWeatherUpdate_App_Task, &googleWeather_Task_H, "GoogleWeather", 10240);

void googleWeatherUpdate_App_Task(void* dApplication){
  Applications *thisApp = (Applications *)dApplication;
  thisApp->app_EncoderFn_ptr = brightnessControl;
  thisApp->app_ButtonFn_ptr = changeGoogleWeatherLocation;
  ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(setGoogleWeatherLocation);
  appsInitialization(thisApp, statusBarClock_Sv);
  //************************************************************************************ */
  read_struct_from_nvs("googleWeather", &currentGoogleWeatherData, sizeof(GoogleWeatherData_t));

while (THIS_APP_IS_ACTIVE == pdTRUE) {

    while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (THIS_APP_IS_ACTIVE == pdTRUE) {}

}

  kill_This_App(thisApp);
}

void changeGoogleWeatherLocation(button_event_t button_Data){
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

void setGoogleWeatherLocation(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["location"];
    write_struct_to_nvs("googleWeather", &currentGoogleWeatherData, sizeof(GoogleWeatherData_t));
    ble_Application_Command_Respond_Success(googleWeatherAppRoute, cmdNumber, pdPASS);
}
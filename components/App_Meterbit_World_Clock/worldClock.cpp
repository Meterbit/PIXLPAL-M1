#include <Arduino.h>
#include <HTTPClient.h>
#include "mtb_github.h"
#include "mtb_text_scroll.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "mtb_nvs.h"
#include "mtb_engine.h"
#include "worldClock.h"

WorldClock_Data_t worldClockCities{
    "New York",
    "London",
    "Tokyo",
    "Sydney",
    "Moscow",
    "Los Angeles"
};

EXT_RAM_BSS_ATTR TaskHandle_t worldClock_Task_H = NULL;
void worldClock_App_Task(void *);
// supporting functions

// button and encoder functions
void selectAM_PMButton(button_event_t button_Data);

// bluetooth functions
void setWorldClockCities(DynamicJsonDocument&);

EXT_RAM_BSS_ATTR Applications_StatusBar *worldClock_App = new Applications_StatusBar(worldClock_App_Task, &worldClock_Task_H, "world Clock", 10240);

void worldClock_App_Task(void* dApplication){
  Applications *thisApp = (Applications *)dApplication;
  thisApp->app_EncoderFn_ptr = brightnessControl;
  thisApp->app_ButtonFn_ptr = selectAM_PMButton;
  ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(setWorldClockCities);
  appsInitialization(thisApp);
  //************************************************************************************ */
  read_struct_from_nvs("worldClock", &worldClockCities, sizeof(WorldClock_Data_t));

while (THIS_APP_IS_ACTIVE == pdTRUE) {

    while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (THIS_APP_IS_ACTIVE == pdTRUE) {}

}

  kill_This_App(thisApp);
}

void selectAM_PMButton(button_event_t button_Data){
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

void setWorldClockCities(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["duration"];

    write_struct_to_nvs("worldClock", &worldClockCities, sizeof(WorldClock_Data_t));
    ble_Application_Command_Respond_Success(worldClockAppRoute, cmdNumber, pdPASS);
}
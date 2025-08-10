#include <Arduino.h>
#include <HTTPClient.h>
#include "mtb_github.h"
#include "mtb_text_scroll.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "mtb_nvs.h"
#include "openMeteo.h"
#include "mtb_engine.h"

OpenMeteoData_t currentOpenMeteoData = {
    "Lagos, Nigeria"
};

EXT_RAM_BSS_ATTR TaskHandle_t openMeteo_Task_H = NULL;
void openMeteoUpdate_App_Task(void *);
// supporting functions

// button and encoder functions
void changeOpenMeteoLocation(button_event_t button_Data);

// bluetooth functions
void setOpenMeteoLocation(DynamicJsonDocument&);

EXT_RAM_BSS_ATTR Applications_StatusBar *openMeteo_App = new Applications_StatusBar(openMeteoUpdate_App_Task, &openMeteo_Task_H, "Open Meteo", 10240);

void openMeteoUpdate_App_Task(void* dApplication){
  Applications *thisApp = (Applications *)dApplication;
  thisApp->app_EncoderFn_ptr = brightnessControl;
  thisApp->app_ButtonFn_ptr = changeOpenMeteoLocation;
  ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(setOpenMeteoLocation);
  appsInitialization(thisApp, statusBarClock_Sv);
  //************************************************************************************ */
  read_struct_from_nvs("openMeteo", &currentOpenMeteoData, sizeof(OpenMeteoData_t));

while (THIS_APP_IS_ACTIVE == pdTRUE) {

    while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (THIS_APP_IS_ACTIVE == pdTRUE) {}

}

  kill_This_App(thisApp);
}

void changeOpenMeteoLocation(button_event_t button_Data){
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

void setOpenMeteoLocation(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["location"];
    write_struct_to_nvs("openMeteo", &currentOpenMeteoData, sizeof(OpenMeteoData_t));
    ble_Application_Command_Respond_Success(openMeteoAppRoute, cmdNumber, pdPASS);
}
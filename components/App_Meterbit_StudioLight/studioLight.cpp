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
#include "studioLight.h"

StudioLight_Data_t studioLightsInfo;

EXT_RAM_BSS_ATTR TaskHandle_t studioLight_Task_H = NULL;
void studioLight_App_Task(void *);
// supporting functions

// button and encoder functions
void selectStudioLightPatternButton(button_event_t button_Data);

// bluetooth functions
void setStudioLightColors(DynamicJsonDocument&);

EXT_RAM_BSS_ATTR Applications_FullScreen *studioLight_App = new Applications_FullScreen(studioLight_App_Task, &studioLight_Task_H, "studioLight", 10240);

void studioLight_App_Task(void* dApplication){
  Applications *thisApp = (Applications *)dApplication;
  thisApp->app_EncoderFn_ptr = brightnessControl;
  thisApp->app_ButtonFn_ptr = selectStudioLightPatternButton;
  ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(setStudioLightColors);
  appsInitialization(thisApp);
  //************************************************************************************ */
  read_struct_from_nvs("studioLight", &studioLightsInfo, sizeof(StudioLight_Data_t));

while (THIS_APP_IS_ACTIVE == pdTRUE) {

    while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (THIS_APP_IS_ACTIVE == pdTRUE) {}

}

  kill_This_App(thisApp);
}

void selectStudioLightPatternButton(button_event_t button_Data){
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

void setStudioLightColors(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["duration"];

    write_struct_to_nvs("studioLight", &studioLightsInfo, sizeof(StudioLight_Data_t));
    ble_Application_Command_Respond_Success(studioLightAppRoute, cmdNumber, pdPASS);
}
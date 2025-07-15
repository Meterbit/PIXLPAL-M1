#include <Arduino.h>
#include <HTTPClient.h>
#include "mtbGithubStorage.h"
#include "scrollMsgs.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "nvsMem.h"
#include "mtbApps.h"
#include "worldFlags.h"
#include "workerWorldFlagsFns.h"
#include "psram_allocator.h"

WorldFlags_Data_t worldFlagsInfo;

EXT_RAM_BSS_ATTR TaskHandle_t worldFlags_Task_H = NULL;
void worldFlags_App_Task(void *);
// supporting functions

// button and encoder functions
void changeWorldFlagButton(button_event_t button_Data);

// bluetooth functions
void selectDisplayFlag(DynamicJsonDocument&);
void showCountryName(DynamicJsonDocument&);
void showRandomFlags(DynamicJsonDocument&);
void setFlagChangeIntv(DynamicJsonDocument&);

EXT_RAM_BSS_ATTR Applications_FullScreen *worldFlags_App = new Applications_FullScreen(worldFlags_App_Task, &worldFlags_Task_H, "worldFlags", 4096);

void worldFlags_App_Task(void* dApplication){
  Applications *thisApp = (Applications *)dApplication;
  thisApp->app_EncoderFn_ptr = brightnessControl;
  thisApp->app_ButtonFn_ptr = changeWorldFlagButton;
  ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(selectDisplayFlag, showCountryName, showRandomFlags, setFlagChangeIntv);
  appsInitialization(thisApp);
  //************************************************************************************ */
  read_struct_from_nvs("worldFlags", &worldFlagsInfo, sizeof(WorldFlags_Data_t));

    SVG_OnlineImage_t holderImage{
        .imageLink = "placeHolder",
        .xAxis = 20,
        .yAxis = 0,
        .scale = 1
    };

while (THIS_APP_IS_ACTIVE == pdTRUE) {

    while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);

    while (THIS_APP_IS_ACTIVE == pdTRUE) {
        strcpy(holderImage.imageLink, getRandomFlag4x3().c_str());
        drawOnlineSVG(holderImage);
        delay(10000);
    }

}

  kill_This_App(thisApp);
}

void changeWorldFlagButton(button_event_t button_Data){
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

void selectDisplayFlag(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["duration"];

    write_struct_to_nvs("worldFlags", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
    ble_Application_Command_Respond_Success(worldFlagsAppRoute, cmdNumber, pdPASS);
}

void showCountryName(DynamicJsonDocument&){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["duration"];

    write_struct_to_nvs("worldFlags", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
    ble_Application_Command_Respond_Success(worldFlagsAppRoute, cmdNumber, pdPASS);
}

void showRandomFlags(DynamicJsonDocument&){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["duration"];

    write_struct_to_nvs("worldFlags", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
    ble_Application_Command_Respond_Success(worldFlagsAppRoute, cmdNumber, pdPASS);
}

void setFlagChangeIntv(DynamicJsonDocument&){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["duration"];

    write_struct_to_nvs("worldFlags", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
    ble_Application_Command_Respond_Success(worldFlagsAppRoute, cmdNumber, pdPASS);
}
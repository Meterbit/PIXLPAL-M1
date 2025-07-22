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

void wipeFlagBackground(void);

// button and encoder functions
void changeWorldFlagButton(button_event_t button_Data);

// bluetooth functions
void selectDisplayFlag(DynamicJsonDocument&);
void selectPreferredFlags(DynamicJsonDocument&);
void cycleAllFlags(DynamicJsonDocument&);
void showCountryName(DynamicJsonDocument&);
void setFlagChangeIntv(DynamicJsonDocument&);

EXT_RAM_BSS_ATTR Applications_FullScreen *worldFlags_App = new Applications_FullScreen(worldFlags_App_Task, &worldFlags_Task_H, "worlfFlagsApp", 4096);

void worldFlags_App_Task(void* dApplication){
  Applications *thisApp = (Applications *)dApplication;
  thisApp->app_EncoderFn_ptr = brightnessControl;
  thisApp->app_ButtonFn_ptr = changeWorldFlagButton;
  ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(selectDisplayFlag, selectPreferredFlags, cycleAllFlags, showCountryName, setFlagChangeIntv);
  appsInitialization(thisApp);
  //************************************************************************************ */
  read_struct_from_nvs("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));

    SVG_OnlineImage_t imageHolder({"placeHolder", 16, 0, 1});

while (THIS_APP_IS_ACTIVE == pdTRUE) {

    while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);

        strcpy(imageHolder.imageLink, getFlag4x3ByCountry(worldFlagsInfo.countryName).c_str());
        drawOnlineSVGs(&imageHolder, 1, wipeFlagBackground); 

    while (THIS_APP_IS_ACTIVE == pdTRUE) {
        if (worldFlagsInfo.cycleAllFlags == true) {
            uint8_t changeIntv = worldFlagsInfo.flagChangeIntv;
            strcpy(imageHolder.imageLink, getRandomFlag4x3().c_str());
            drawOnlineSVGs(&imageHolder, 1, wipeFlagBackground);
            while(changeIntv-->0 && Applications::internetConnectStatus == true && THIS_APP_IS_ACTIVE == pdTRUE) delay(1000);
        } else delay(1000);
        if (worldFlagsInfo.showCountryName == true) {
            
        } 
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

void wipeFlagBackground(void){
    dma_display->fillScreen(dma_display->color565(0, 0, 16)); // Clear the entire screen
}
//************************************************************************************ */
//************************************************************************************ */

void selectDisplayFlag(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    const char* countryFlag = dCommand["countryName"];

    printf("Select Country: %s \n", countryFlag);

    SVG_OnlineImage_t imageHolder({"placeHolder", 16, 0, 1});
    
    strcpy(worldFlagsInfo.countryName, countryFlag);
    strcpy(imageHolder.imageLink, getFlag4x3ByCountry(countryFlag).c_str());
    drawOnlineSVGs(&imageHolder, 1, wipeFlagBackground); 

    write_struct_to_nvs("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
    ble_Application_Command_Respond_Success(worldFlagsAppRoute, cmdNumber, pdPASS);
}

void selectPreferredFlags(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    //write_struct_to_nvs("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
    ble_Application_Command_Respond_Success(worldFlagsAppRoute, cmdNumber, pdPASS);
}

void cycleAllFlags(DynamicJsonDocument&){
    uint8_t cmdNumber = dCommand["app_command"];
    worldFlagsInfo.cycleAllFlags = dCommand["cycleFlags"].as<bool>();
    worldFlagsInfo.flagChangeIntv = dCommand["dInterval"].as<uint8_t>();
    write_struct_to_nvs("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
    ble_Application_Command_Respond_Success(worldFlagsAppRoute, cmdNumber, pdPASS);
}

void showCountryName(DynamicJsonDocument&){
    uint8_t cmdNumber = dCommand["app_command"];
    worldFlagsInfo.showCountryName = dCommand["showData"].as<bool>();
    write_struct_to_nvs("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
    ble_Application_Command_Respond_Success(worldFlagsAppRoute, cmdNumber, pdPASS);
}

void setFlagChangeIntv(DynamicJsonDocument&){
    uint8_t cmdNumber = dCommand["app_command"];
    worldFlagsInfo.flagChangeIntv = dCommand["dInterval"].as<uint8_t>();
    write_struct_to_nvs("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
    ble_Application_Command_Respond_Success(worldFlagsAppRoute, cmdNumber, pdPASS);
}




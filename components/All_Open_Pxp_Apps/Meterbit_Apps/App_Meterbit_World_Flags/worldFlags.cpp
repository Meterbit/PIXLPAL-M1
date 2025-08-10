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
#include "worldFlags.h"
#include "workerWorldFlagsFns.h"
#include "psram_allocator.h"

static const char TAG[] = "WORLD_FLAGS";

WorldFlags_Data_t worldFlagsInfo;

EXT_RAM_BSS_ATTR TaskHandle_t worldFlags_Task_H = NULL;
void worldFlags_App_Task(void *);
// supporting functions

void wipeFlagBackground(void);

// button and encoder functions
void changeWorldFlagButton(button_event_t button_Data);

// bluetooth functions
void selectDisplayFlag(JsonDocument&);
void selectPreferredFlags(JsonDocument&);
void cycleAllFlags(JsonDocument&);
void showCountryName(JsonDocument&);
void setFlagChangeIntv(JsonDocument&);

EXT_RAM_BSS_ATTR Applications_FullScreen *worldFlags_App = new Applications_FullScreen(worldFlags_App_Task, &worldFlags_Task_H, "worlfFlagsApp", 4096);

void worldFlags_App_Task(void* dApplication){
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_EncoderFn_ptr = mtb_Brightness_Control;
  thisApp->mtb_App_ButtonFn_ptr = changeWorldFlagButton;
  mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(selectDisplayFlag, selectPreferredFlags, cycleAllFlags, showCountryName, setFlagChangeIntv);
  mtb_App_Init(thisApp);
  //************************************************************************************ */
  mtb_Read_Nvs_Struct("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));

    SVG_OnlineImage_t imageHolder({"placeHolder", 16, 0, 1});

while (MTB_APP_IS_ACTIVE == pdTRUE){

    while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);

        strcpy(imageHolder.imageLink, getFlag4x3ByCountry(worldFlagsInfo.countryName).c_str());
        drawOnlineSVGs(&imageHolder, 1, wipeFlagBackground); 

    while (MTB_APP_IS_ACTIVE == pdTRUE && Mtb_Applications::internetConnectStatus == true) {
        if (worldFlagsInfo.cycleAllFlags == true) {
            uint8_t changeIntv = worldFlagsInfo.flagChangeIntv;
            strcpy(imageHolder.imageLink, getRandomFlag4x3().c_str());
            drawOnlineSVGs(&imageHolder, 1, wipeFlagBackground);
            while(changeIntv-->0 && Mtb_Applications::internetConnectStatus == true && MTB_APP_IS_ACTIVE == pdTRUE) delay(1000);
        } else delay(1000);
        if (worldFlagsInfo.showCountryName == true) {
            
        } 
    }
}
  mtb_End_This_App(thisApp);
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

void wipeFlagBackground(void){
    dma_display->fillScreen(dma_display->color565(0, 0, 16)); // Clear the entire screen
}
//************************************************************************************ */
//************************************************************************************ */

void selectDisplayFlag(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    const char* countryFlag = dCommand["countryName"];

    ESP_LOGI(TAG, "Select Country: %s \n", countryFlag);

    SVG_OnlineImage_t imageHolder({"placeHolder", 16, 0, 1});
    
    strcpy(worldFlagsInfo.countryName, countryFlag);
    strcpy(imageHolder.imageLink, getFlag4x3ByCountry(countryFlag).c_str());
    drawOnlineSVGs(&imageHolder, 1, wipeFlagBackground); 

    mtb_Write_Nvs_Struct("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
    mtb_Ble_App_Cmd_Respond_Success(worldFlagsAppRoute, cmdNumber, pdPASS);
}

void selectPreferredFlags(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    //mtb_Write_Nvs_Struct("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
    mtb_Ble_App_Cmd_Respond_Success(worldFlagsAppRoute, cmdNumber, pdPASS);
}

void cycleAllFlags(JsonDocument&){
    uint8_t cmdNumber = dCommand["app_command"];
    worldFlagsInfo.cycleAllFlags = dCommand["cycleFlags"].as<bool>();
    worldFlagsInfo.flagChangeIntv = dCommand["dInterval"].as<uint8_t>();
    mtb_Write_Nvs_Struct("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
    mtb_Ble_App_Cmd_Respond_Success(worldFlagsAppRoute, cmdNumber, pdPASS);
}

void showCountryName(JsonDocument&){
    uint8_t cmdNumber = dCommand["app_command"];
    worldFlagsInfo.showCountryName = dCommand["showData"].as<bool>();
    mtb_Write_Nvs_Struct("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
    mtb_Ble_App_Cmd_Respond_Success(worldFlagsAppRoute, cmdNumber, pdPASS);
}

void setFlagChangeIntv(JsonDocument&){
    uint8_t cmdNumber = dCommand["app_command"];
    worldFlagsInfo.flagChangeIntv = dCommand["dInterval"].as<uint8_t>();
    mtb_Write_Nvs_Struct("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
    mtb_Ble_App_Cmd_Respond_Success(worldFlagsAppRoute, cmdNumber, pdPASS);
}




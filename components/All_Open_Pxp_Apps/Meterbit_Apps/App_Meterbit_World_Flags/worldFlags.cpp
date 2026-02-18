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

EXT_RAM_BSS_ATTR WorldFlags_Data_t worldFlagsInfo;

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

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *worldFlags_App = new Mtb_Applications_FullScreen(worldFlags_App_Task, &worldFlags_Task_H, "worldFlagsApp", 8192);

void worldFlags_App_Task(void* dApplication){
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_EncoderFn_ptr = mtb_Brightness_Control;
  thisApp->mtb_App_ButtonFn_ptr = changeWorldFlagButton;
  mtb_App_BleComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(selectDisplayFlag, selectPreferredFlags, cycleAllFlags, showCountryName, setFlagChangeIntv);
  mtb_App_Init(thisApp);
  //************************************************************************************ */
  worldFlagsInfo = (WorldFlags_Data_t){
        "Nigeria",    // 
        100,       // 0-255
        true,         // true or false
        false       // true or false
    };
  mtb_Read_Nvs_Struct("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));

    std::string country;
    std::string flagLink;
    Mtb_OnlineImage_t large_Flag({"link placeHolder", 16, 0, 1});
    Mtb_OnlineImage_t small_Flag({"link placeHolder", 40, 10, 2});
    Mtb_CentreText_t dispCountryName(65, 57, Terminal8x12, WHITE);

while (MTB_APP_IS_ACTIVE == pdTRUE){

    while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);

        strcpy(large_Flag.imageLink, getFlag4x3ByCountry(worldFlagsInfo.countryName).c_str());
        mtb_Draw_Online_Svg(&large_Flag, 1, wipeFlagBackground);

    while (MTB_APP_IS_ACTIVE == pdTRUE && Mtb_Applications::internetConnectStatus == true) {
        if (worldFlagsInfo.cycleAllFlags == true) {
            uint8_t changeIntv = 5;
            // uint8_t changeIntv = worldFlagsInfo.flagChangeIntv;
            getRandomCountryAndFlag4x3(country, flagLink);
            strcpy(large_Flag.imageLink, flagLink.c_str());
            mtb_Draw_Online_Svg(&large_Flag, 1, wipeFlagBackground);
            while(changeIntv-->0 && Mtb_Applications::internetConnectStatus == true && MTB_APP_IS_ACTIVE == pdTRUE) delay(1000);
        } else delay(1000);
        if (worldFlagsInfo.showCountryName == true) {
            uint8_t changeIntv = worldFlagsInfo.flagChangeIntv/2;
            strcpy(small_Flag.imageLink, large_Flag.imageLink);
            mtb_Draw_Online_Svg(&small_Flag, 1, wipeFlagBackground);
            dispCountryName.mtb_Write_Colored_String(country.c_str(), WHITE, mtb_Panel_Color565(0, 0, 16));
            while(changeIntv-->0 && Mtb_Applications::internetConnectStatus == true && MTB_APP_IS_ACTIVE == pdTRUE) delay(1000);
        } 
    }
}
  mtb_Delete_This_App(thisApp);
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
    mtb_Panel_Fill_Screen(mtb_Panel_Color565(0, 0, 16)); // Clear the entire screen
}
//************************************************************************************ */
//************************************************************************************ */

void selectDisplayFlag(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    const char* countryFlag = dCommand["countryName"];

    ESP_LOGI(TAG, "Select Country: %s \n", countryFlag);

    Mtb_OnlineImage_t imageHolder({"placeHolder", 16, 0, 1});
    
    strcpy(worldFlagsInfo.countryName, countryFlag);
    strcpy(imageHolder.imageLink, getFlag4x3ByCountry(countryFlag).c_str());
    mtb_Draw_Online_Svg(&imageHolder, 1, wipeFlagBackground); 

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




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
#include "workerWorldFlagsFns.h"
#include "psram_allocator.h"

static const char TAG[] = "WORLD_FLAGS";

struct WorldFlags_Data_t {
char countryName[100] = "Nigeria";    // 
uint8_t flagChangeIntv = 100;       // 0-255
bool cycleAllFlags = true;         // true or false
bool showCountryName = false;       // true or false
};

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

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *worldFlags_App = new Mtb_Applications_FullScreen(worldFlags_App_Task, &worldFlags_Task_H, "worldFlagsApp", 3072);

void worldFlags_App_Task(void* dApplication){
  Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
  THIS_APP->mtb_App_Set_EC11_Cb_Fns(changeWorldFlagButton, mtb_Brightness_Control);
  THIS_APP->mtb_App_Set_Ble_Comm_Sv_Fns(selectDisplayFlag, selectPreferredFlags, cycleAllFlags, showCountryName, setFlagChangeIntv);
  THIS_APP->mtb_App_Init();
  //************************************************************************************ */
  worldFlagsInfo = (WorldFlags_Data_t){
        "Nigeria",    // Country Name
        100,       // Flag Change interval in seconds
        true,         // Show Country Name
        false       // Cycle All Flags
    };
  mtb_Read_Nvs_Struct("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));

    std::string country;
    std::string flagLink;
    Mtb_OnlineImage_t large_Flag({"link placeHolder", 16, 0, 1});
    Mtb_OnlineImage_t small_Flag({"link placeHolder", 40, 10, 2});
    Mtb_CentreText_t dispCountryName(65, 57, Terminal8x12, WHITE);

while (THIS_APP_IS_ACTIVE == pdTRUE){

    while ((Mtb_Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);

        strcpy(large_Flag.imageLink, getFlag4x3ByCountry(worldFlagsInfo.countryName).c_str());
        mtb_Draw_Online_Svg(&large_Flag, 1, wipeFlagBackground);

    while (THIS_APP_IS_ACTIVE == pdTRUE && Mtb_Applications::internetConnectStatus == true) {
        if (worldFlagsInfo.cycleAllFlags == true) {
            uint8_t changeLargeFlagIntv = worldFlagsInfo.flagChangeIntv;
            getRandomCountryAndFlag4x3(country, flagLink);
            strcpy(large_Flag.imageLink, flagLink.c_str());
            mtb_Draw_Online_Svg(&large_Flag, 1, wipeFlagBackground);
            while(changeLargeFlagIntv-->0 && THIS_APP_IS_ACTIVE == pdTRUE) delay(1000);
        } else delay(1000);
        if (worldFlagsInfo.showCountryName == true) {
            uint8_t changeSmallFlagIntv = 10;
            //uint8_t changeIntv = worldFlagsInfo.flagChangeIntv/2;
            strcpy(small_Flag.imageLink, large_Flag.imageLink);
            mtb_Draw_Online_Svg(&small_Flag, 1, wipeFlagBackground);
            dispCountryName.mtb_Write_Colored_Text(country.c_str(), WHITE, mtb_Panel_Color565(0, 0, 16));
            while(changeSmallFlagIntv-->0 && THIS_APP_IS_ACTIVE == pdTRUE) delay(1000);
        } 
    }
}
  mtb_Delete_This_App(THIS_APP);
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
    const char* countryFlag = dCommand["countryName"];
    ESP_LOGI(TAG, "Select Country: %s \n", countryFlag);
    Mtb_OnlineImage_t imageHolder({"placeHolder", 16, 0, 1});
    strcpy(worldFlagsInfo.countryName, countryFlag);
    strcpy(imageHolder.imageLink, getFlag4x3ByCountry(countryFlag).c_str());
    mtb_Draw_Online_Svg(&imageHolder, 1, wipeFlagBackground); 
    mtb_Write_Nvs_Struct("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
}

void selectPreferredFlags(JsonDocument& dCommand){
    //mtb_Write_Nvs_Struct("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
}

void cycleAllFlags(JsonDocument& dCommand){

    worldFlagsInfo.cycleAllFlags = dCommand["cycleFlags"].as<bool>();
    worldFlagsInfo.showCountryName = dCommand["showData"].as<bool>();
    worldFlagsInfo.flagChangeIntv = dCommand["dInterval"].as<uint8_t>();
    mtb_Write_Nvs_Struct("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
}

void showCountryName(JsonDocument&){
    worldFlagsInfo.showCountryName = dCommand["showData"].as<bool>();
    mtb_Write_Nvs_Struct("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
}

void setFlagChangeIntv(JsonDocument& dCommand){
    worldFlagsInfo.flagChangeIntv = dCommand["dInterval"].as<uint8_t>();
    mtb_Write_Nvs_Struct("worldFlagsData", &worldFlagsInfo, sizeof(WorldFlags_Data_t));
}




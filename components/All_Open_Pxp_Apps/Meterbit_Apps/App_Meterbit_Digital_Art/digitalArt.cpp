#include <Arduino.h>
#include <HTTPClient.h>
//#include "mtb_github.h"
#include "mtb_text_scroll.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "mtb_nvs.h"
#include "mtb_engine.h"
#include "mtb_pixel_art_png.h"
#include "psram_allocator.h"
#include "my_secret_keys.h"
#include "mtb_github.h"

static const char TAG[] = "DIGITAL_ART_APP";

struct Digital_Data_t {
char displayArtName[200];
uint8_t displayArtChangeIntv;
bool cycleAlldigitalArts;
};

EXT_RAM_BSS_ATTR Digital_Data_t digitalArtInfo;
EXT_RAM_BSS_ATTR TaskHandle_t digitalArt_Task_H = NULL;
void digitalArt_App_Task(void *);

// digital art helper functions
void getRamdomDigitalArtName(char* artName);
std::string constructPathFromName(const char* artName);

// button and encoder functions
void changeDigitalArtButton(button_event_t button_Data);

// bluetooth functions
void next_DigitalArt(JsonDocument&);
void cycleAllDigitalArt(JsonDocument&);
void setDigitalChangeIntv(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *digitalArt_App;
Mtb_Applications* digitalArt_App_GetInstance() {
    if (!digitalArt_App) digitalArt_App = new Mtb_Applications_FullScreen(digitalArt_App_Task, &digitalArt_Task_H, "DigitalArt App", {6,2}, 4096);
    return digitalArt_App;
}

MTB_REGISTER_APP(digitalArt_App, 6, 2)

void digitalArt_App_Task(void* dApplication){
  Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
  THIS_APP->mtb_App_Set_EC11_Cb_Fns(changeDigitalArtButton, mtb_Brightness_Control);
  THIS_APP->mtb_App_Set_Ble_Comm_Fns(cycleAllDigitalArt, setDigitalChangeIntv, next_DigitalArt);
  THIS_APP->mtb_App_Init();
  //************************************************************************************ */

    digitalArtInfo = (Digital_Data_t){
        "pixellab-African-neighborhood-street-wi-1778402642758.png",
        100,
        true,
    };

    mtb_Read_Nvs_Struct("digiArtData", &digitalArtInfo, sizeof(Digital_Data_t));

    uint8_t* buffer = nullptr;
    size_t imageSize = 0;

    while (THIS_APP_IS_ACTIVE == pdTRUE){

        while ((Mtb_Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);

        mtb_Download_Github_File_To_PSRAM(constructPathFromName(digitalArtInfo.displayArtName).c_str(), &buffer, &imageSize, "Meterbit", "METERBIT_PRIV_RES", github_Token);
        mtb_Draw_PSRAM_Png(&buffer, &imageSize);

        while (THIS_APP_IS_ACTIVE == pdTRUE && Mtb_Applications::internetConnectStatus == true) {
            if (digitalArtInfo.cycleAlldigitalArts == true) {
                uint8_t changeArtIntv = digitalArtInfo.displayArtChangeIntv;
                getRamdomDigitalArtName(digitalArtInfo.displayArtName);
                mtb_Download_Github_File_To_PSRAM(constructPathFromName(digitalArtInfo.displayArtName).c_str(), &buffer, &imageSize, "Meterbit", "METERBIT_PRIV_RES", github_Token);
                mtb_Draw_PSRAM_Png(&buffer, &imageSize);
                while(changeArtIntv-->0 && THIS_APP_IS_ACTIVE == pdTRUE) delay(1000);
            } else delay(1000);
        }
    }
  mtb_Delete_This_App(THIS_APP);
}

void changeDigitalArtButton(button_event_t button_Data){
    switch (button_Data.type){
    case BUTTON_RELEASED:
        break;
    case BUTTON_PRESSED:
        break;
    case BUTTON_PRESSED_LONG:
        digitalArtInfo.cycleAlldigitalArts = !digitalArtInfo.cycleAlldigitalArts;
        mtb_Write_Nvs_Struct("digiArtData", &digitalArtInfo, sizeof(Digital_Data_t));
        break;
    case BUTTON_CLICKED:
        switch (button_Data.count){
        case 1: {
            getRamdomDigitalArtName(digitalArtInfo.displayArtName);
            uint8_t* buf = nullptr;
            size_t sz = 0;
            mtb_Download_Github_File_To_PSRAM(constructPathFromName(digitalArtInfo.displayArtName).c_str(), &buf, &sz, "Meterbit", "METERBIT_PRIV_RES", github_Token);
            mtb_Draw_PSRAM_Png(&buf, &sz);
            break;
        }
        default:
            break;
        }
        break;
    default:
        break;
    }
}

//************************************************************************************ */

void getRamdomDigitalArtName(char* artName){
    JsonDocument doc;
    deserializeJson(doc, json_Github_Pixel_Art_Pngs);
    JsonArray names = doc["names"].as<JsonArray>();
    uint32_t count = names.size();
    if (count == 0) return;
    uint32_t idx = esp_random() % count;
    strlcpy(artName, names[idx].as<const char*>(), 200);
}

std::string constructPathFromName(const char* artName) {
    return std::string("Static_Pixel_Art/") + artName;
}

// bluetooth functions

void cycleAllDigitalArt(JsonDocument& dCommand){
    digitalArtInfo.cycleAlldigitalArts = dCommand["cycleArts"].as<bool>();
    digitalArtInfo.displayArtChangeIntv = dCommand["dInterval"].as<uint8_t>();
    mtb_Write_Nvs_Struct("digiArtData", &digitalArtInfo, sizeof(Digital_Data_t));
}

void setDigitalChangeIntv(JsonDocument& dCommand){
    digitalArtInfo.displayArtChangeIntv = dCommand["dInterval"].as<uint8_t>();
    mtb_Write_Nvs_Struct("digiArtData", &digitalArtInfo, sizeof(Digital_Data_t));
}

void next_DigitalArt(JsonDocument& dCommand){
    const char* artName = dCommand["artName"];
    if (artName) strlcpy(digitalArtInfo.displayArtName, artName, 200);
    else getRamdomDigitalArtName(digitalArtInfo.displayArtName);
    ESP_LOGI(TAG, "Display Art: %s", digitalArtInfo.displayArtName);
    uint8_t* buf = nullptr;
    size_t sz = 0;
    mtb_Download_Github_File_To_PSRAM(constructPathFromName(digitalArtInfo.displayArtName).c_str(), &buf, &sz, "Meterbit", "METERBIT_PRIV_RES", github_Token);
    mtb_Draw_PSRAM_Png(&buf, &sz);
    mtb_Write_Nvs_Struct("digiArtData", &digitalArtInfo, sizeof(Digital_Data_t));
}
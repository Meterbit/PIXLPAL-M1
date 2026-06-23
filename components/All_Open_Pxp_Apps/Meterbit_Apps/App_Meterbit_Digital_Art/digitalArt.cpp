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

static const char TAG[] = "DIGITAL_ART_APP";


EXT_RAM_BSS_ATTR TaskHandle_t digitalArt_Task_H = NULL;
void digitalArt_App_Task(void *);
// supporting functions


// button and encoder functions
void changeDigitalArtButton(button_event_t button_Data);

// bluetooth functions
void selectDisplayFlag(JsonDocument&);
void selectPreferredFlags(JsonDocument&);
void cycleAllFlags(JsonDocument&);
void showCountryName(JsonDocument&);
void setFlagChangeIntv(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *digitalArt_App;
Mtb_Applications* digitalArt_App_GetInstance() {
    if (!digitalArt_App) digitalArt_App = new Mtb_Applications_FullScreen(digitalArt_App_Task, &digitalArt_Task_H, "digitalArtApp", {6,1}, 3072);
    return digitalArt_App;
}

MTB_REGISTER_APP(digitalArt_App, 6, 1)

void digitalArt_App_Task(void* dApplication){
  Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
  THIS_APP->mtb_App_Set_EC11_Cb_Fns(changeDigitalArtButton, mtb_Brightness_Control);
  //THIS_APP->mtb_App_Set_Ble_Comm_Fns();
  THIS_APP->mtb_App_Init();
  //************************************************************************************ */


while (THIS_APP_IS_ACTIVE == pdTRUE){

    while ((Mtb_Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);

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


//************************************************************************************ */




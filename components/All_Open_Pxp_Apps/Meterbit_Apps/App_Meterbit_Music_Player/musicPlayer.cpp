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

struct MusicPlayer_Data_t {
uint16_t trackNumber;
};

EXT_RAM_BSS_ATTR MusicPlayer_Data_t musicPlayerData;

EXT_RAM_BSS_ATTR TaskHandle_t musicPlayer_Task_H = NULL;
void musicPlayer_App_Task(void *);
// supporting functions

// button and encoder functions
void nextTrackButton(button_event_t button_Data);

// bluetooth functions
void selectNext_PreviousTrack(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *musicPlayer_App = new Mtb_Applications_StatusBar(musicPlayer_App_Task, &musicPlayer_Task_H, "musicPlayer", 4096, false);

void musicPlayer_App_Task(void* dApplication){
  Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
  THIS_APP->mtb_App_Set_EC11_Cb_Fns(nextTrackButton, mtb_Vol_Control_Encoder);
  THIS_APP->mtb_App_Set_Ble_Comm_Sv_Fns(selectNext_PreviousTrack);
  THIS_APP->mtb_App_Init(mtb_Usb_Mass_Storage_Sv, mtb_Audio_Out_Sv);
  //************************************************************************************ */
  musicPlayerData = (MusicPlayer_Data_t){1};
  mtb_Read_Nvs_Struct("musicPlayer", &musicPlayerData, sizeof(MusicPlayer_Data_t));

while (THIS_APP_IS_ACTIVE == pdTRUE) {

    while ((Mtb_Applications::usbPenDriveConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (THIS_APP_IS_ACTIVE == pdTRUE) {

    }

}

  mtb_Delete_This_App(THIS_APP);
}

void nextTrackButton(button_event_t button_Data){
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

void selectNext_PreviousTrack(JsonDocument& dCommand){
    mtb_Write_Nvs_Struct("musicPlayer", &musicPlayerData, sizeof(MusicPlayer_Data_t));
}
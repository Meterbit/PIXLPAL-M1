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
#include "musicPlayer.h"

MusicPlayer_Data_t musicPlayerData;

EXT_RAM_BSS_ATTR TaskHandle_t musicPlayer_Task_H = NULL;
void musicPlayer_App_Task(void *);
// supporting functions

// button and encoder functions
void nextTrackButton(button_event_t button_Data);

// bluetooth functions
void selectNext_PreviousTrack(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *musicPlayer_App = new Mtb_Applications_StatusBar(musicPlayer_App_Task, &musicPlayer_Task_H, "musicPlayer", 10240);

void musicPlayer_App_Task(void* dApplication){
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_EncoderFn_ptr = mtb_Vol_Control_Encoder;
  thisApp->mtb_App_ButtonFn_ptr = nextTrackButton;
  mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(selectNext_PreviousTrack);
  mtb_App_Init(thisApp);
  //************************************************************************************ */
  mtb_Read_Nvs_Struct("musicPlayer", &musicPlayerData, sizeof(MusicPlayer_Data_t));

while (MTB_APP_IS_ACTIVE == pdTRUE) {

    while ((Mtb_Applications::usbPenDriveConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (MTB_APP_IS_ACTIVE == pdTRUE) {

    }

}

  mtb_End_This_App(thisApp);
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
    uint8_t cmdNumber = dCommand["app_command"];
//    String location = dCommand["duration"];

    mtb_Write_Nvs_Struct("musicPlayer", &musicPlayerData, sizeof(MusicPlayer_Data_t));
    mtb_Ble_App_Cmd_Respond_Success(musicPlayerAppRoute, cmdNumber, pdPASS);
}
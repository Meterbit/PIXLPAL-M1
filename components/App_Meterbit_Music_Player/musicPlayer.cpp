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
void selectNext_PreviousTrack(DynamicJsonDocument&);

EXT_RAM_BSS_ATTR Applications_StatusBar *musicPlayer_App = new Applications_StatusBar(musicPlayer_App_Task, &musicPlayer_Task_H, "musicPlayer", 10240);

void musicPlayer_App_Task(void* dApplication){
  Applications *thisApp = (Applications *)dApplication;
  thisApp->app_EncoderFn_ptr = volumeControl_Encoder;
  thisApp->app_ButtonFn_ptr = nextTrackButton;
  ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(selectNext_PreviousTrack);
  appsInitialization(thisApp);
  //************************************************************************************ */
  read_struct_from_nvs("musicPlayer", &musicPlayerData, sizeof(MusicPlayer_Data_t));

while (THIS_APP_IS_ACTIVE == pdTRUE) {

    while ((Applications::usbPenDriveConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (THIS_APP_IS_ACTIVE == pdTRUE) {

    }

}

  kill_This_App(thisApp);
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

void selectNext_PreviousTrack(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
//    String location = dCommand["duration"];

    write_struct_to_nvs("musicPlayer", &musicPlayerData, sizeof(MusicPlayer_Data_t));
    ble_Application_Command_Respond_Success(musicPlayerAppRoute, cmdNumber, pdPASS);
}
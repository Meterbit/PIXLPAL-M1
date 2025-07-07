#include <HTTPClient.h>
#include "mtbGithubStorage.h"
#include "scrollMsgs.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "nvsMem.h"
#include "mtbApps.h"
#include "realStopwatch.h"

RealStopWatch_Data_t frequentStopwatchTime{60};

EXT_RAM_BSS_ATTR TaskHandle_t realStopwatch_Task_H = NULL;
void realStopwatch_App_Task(void *);
// supporting functions

// button and encoder functions
void selectWatchTimeButton(button_event_t button_Data);
void adjustwatchTimeEncoder(rotary_encoder_rotation_t button_Data);

// bluetooth functions
void setWatchTime(DynamicJsonDocument&);

EXT_RAM_BSS_ATTR Applications_StatusBar *stopWatch_App = new Applications_StatusBar(realStopwatch_App_Task, &realStopwatch_Task_H, "real Stopwatch", 10240);

void realStopwatch_App_Task(void* dApplication){
  Applications *thisApp = (Applications *)dApplication;
  thisApp->app_EncoderFn_ptr = adjustwatchTimeEncoder;
  thisApp->app_ButtonFn_ptr = selectWatchTimeButton;
  ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(setWatchTime);
  appsInitialization(thisApp);
  //************************************************************************************ */
  read_struct_from_nvs("realStopWatch", &frequentStopwatchTime, sizeof(RealStopWatch_Data_t));

while (THIS_APP_IS_ACTIVE == pdTRUE) {

    while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (THIS_APP_IS_ACTIVE == pdTRUE) {}

}

  kill_This_App(thisApp);
}

void selectWatchTimeButton(button_event_t button_Data){
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

void adjustwatchTimeEncoder(rotary_encoder_rotation_t direction){
if (direction == ROT_CLOCKWISE){
    // if(panelBrightness <= 250){ 
    // panelBrightness += 5;
    // dma_display->setBrightness(panelBrightness); // 0-255
    // write_struct_to_nvs("pan_brghnss", &panelBrightness, sizeof(uint8_t));
    // }
    // if(panelBrightness >= 255) do_beep(CLICK_BEEP);
} else if(direction == ROT_COUNTERCLOCKWISE){
    // if(panelBrightness >= 7){
    // panelBrightness -= 5;
    // dma_display->setBrightness(panelBrightness); //0-255
    // write_struct_to_nvs("pan_brghnss", &panelBrightness, sizeof(uint8_t));
    // }
    // if(panelBrightness <= 6) do_beep(CLICK_BEEP);
}
}

void setWatchTime(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["duration"];

    write_struct_to_nvs("realStopWatch", &frequentStopwatchTime, sizeof(RealStopWatch_Data_t));
    ble_Application_Command_Respond_Success(stopWatchAppRoute, cmdNumber, pdPASS);
}
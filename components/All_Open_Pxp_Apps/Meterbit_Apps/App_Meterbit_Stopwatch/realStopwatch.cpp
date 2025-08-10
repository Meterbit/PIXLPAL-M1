#include <HTTPClient.h>
#include "mtb_github.h"
#include "mtb_text_scroll.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "mtb_nvs.h"
#include "mtb_engine.h"
#include "realStopwatch.h"

RealStopWatch_Data_t frequentStopwatchTime{60};

EXT_RAM_BSS_ATTR TaskHandle_t realStopwatch_Task_H = NULL;
void realStopwatch_App_Task(void *);
// supporting functions

// button and encoder functions
void selectWatchTimeButton(button_event_t button_Data);
void adjustwatchTimeEncoder(rotary_encoder_rotation_t button_Data);

// bluetooth functions
void setWatchTime(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *stopWatch_App = new Mtb_Applications_StatusBar(realStopwatch_App_Task, &realStopwatch_Task_H, "real Stopwatch", 10240);

void realStopwatch_App_Task(void* dApplication){
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_EncoderFn_ptr = adjustwatchTimeEncoder;
  thisApp->mtb_App_ButtonFn_ptr = selectWatchTimeButton;
  mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(setWatchTime);
  mtb_App_Init(thisApp);
  //************************************************************************************ */
  mtb_Read_Nvs_Struct("realStopWatch", &frequentStopwatchTime, sizeof(RealStopWatch_Data_t));

while (MTB_APP_IS_ACTIVE == pdTRUE) {

    while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (MTB_APP_IS_ACTIVE == pdTRUE) {}

}

  mtb_End_This_App(thisApp);
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

void adjustwatchTimeEncoder(rotary_encoder_rotation_t direction){
if (direction == ROT_CLOCKWISE){
    // if(panelBrightness <= 250){ 
    // panelBrightness += 5;
    // dma_display->setBrightness(panelBrightness); // 0-255
    // mtb_Write_Nvs_Struct("pan_brghnss", &panelBrightness, sizeof(uint8_t));
    // }
    // if(panelBrightness >= 255) do_beep(CLICK_BEEP);
} else if(direction == ROT_COUNTERCLOCKWISE){
    // if(panelBrightness >= 7){
    // panelBrightness -= 5;
    // dma_display->setBrightness(panelBrightness); //0-255
    // mtb_Write_Nvs_Struct("pan_brghnss", &panelBrightness, sizeof(uint8_t));
    // }
    // if(panelBrightness <= 6) do_beep(CLICK_BEEP);
}
}

void setWatchTime(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["duration"];

    mtb_Write_Nvs_Struct("realStopWatch", &frequentStopwatchTime, sizeof(RealStopWatch_Data_t));
    mtb_Ble_App_Cmd_Respond_Success(stopWatchAppRoute, cmdNumber, pdPASS);
}
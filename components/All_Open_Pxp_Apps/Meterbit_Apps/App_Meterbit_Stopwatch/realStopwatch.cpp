#include <HTTPClient.h>
#include "mtb_github.h"
#include "mtb_text_scroll.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "mtb_nvs.h"
#include "mtb_engine.h"

struct RealStopWatch_Data_t {
uint64_t duration = 60;
};

EXT_RAM_BSS_ATTR RealStopWatch_Data_t frequentStopwatchTime;

EXT_RAM_BSS_ATTR TaskHandle_t realStopwatch_Task_H = NULL;
void realStopwatch_App_Task(void *);
// supporting functions

// button and encoder functions
void selectWatchTimeButton(button_event_t button_Data);
void adjustwatchTimeEncoder(rotary_encoder_rotation_t button_Data);

// bluetooth functions
void setWatchTime(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *stopWatch_App = new Mtb_Applications_StatusBar(realStopwatch_App_Task, &realStopwatch_Task_H, "real Stopwatch", 4096);

void realStopwatch_App_Task(void* dApplication){
  Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
  THIS_APP->mtb_App_Set_EC11_Cb_Fns(selectWatchTimeButton, adjustwatchTimeEncoder);
  THIS_APP->mtb_App_Set_Ble_Comm_Sv_Fns(setWatchTime);
  THIS_APP->mtb_App_Init();
  //************************************************************************************ */
  frequentStopwatchTime = (RealStopWatch_Data_t){60};
  mtb_Read_Nvs_Struct("realStopWatch", &frequentStopwatchTime, sizeof(RealStopWatch_Data_t));

while (THIS_APP_IS_ACTIVE == pdTRUE) {

    while ((Mtb_Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);

    while (THIS_APP_IS_ACTIVE == pdTRUE) {}

}

  mtb_Delete_This_App(THIS_APP);
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
    // mtb_Panel_Set_Brightness(panelBrightness); // 0-255
    // mtb_Write_Nvs_Struct("pan_brghnss", &panelBrightness, sizeof(uint8_t));
    // }
    // if(panelBrightness >= 255) do_beep(CLICK_BEEP);
} else if(direction == ROT_COUNTERCLOCKWISE){
    // if(panelBrightness >= 7){
    // panelBrightness -= 5;
    // mtb_Panel_Set_Brightness(panelBrightness); //0-255
    // mtb_Write_Nvs_Struct("pan_brghnss", &panelBrightness, sizeof(uint8_t));
    // }
    // if(panelBrightness <= 6) do_beep(CLICK_BEEP);
}
}

void setWatchTime(JsonDocument& dCommand){
    String location = dCommand["duration"];
    mtb_Write_Nvs_Struct("realStopWatch", &frequentStopwatchTime, sizeof(RealStopWatch_Data_t));
}
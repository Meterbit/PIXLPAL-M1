#include <Arduino.h>
#include <HTTPClient.h>
#include "mtb_github.h"
#include "mtb_text_scroll.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "mtb_graphics.h"
#include "mtb_nvs.h"
#include "mtb_engine.h"
#include "mtb_buzzer.h"

#define FULLSCREEN_MODE     0
#define HALFSCREEN_MODE     1
#define CYCLE_MODE          2

struct StudioLight_Data_t {
uint16_t studioLightColor[5] = {WHITE, PURPLE, GREEN, YELLOW, BLUE}; //, MAGENTA, PALE_AZURE, EBONY, SWEET_BROWN, SKOBELOFF}; // 
uint8_t studioLightColorMode = FULLSCREEN_MODE;
uint16_t studioLightDuration = 100;
};

EXT_RAM_BSS_ATTR StudioLight_Data_t studioLightsInfo;
EXT_RAM_BSS_ATTR SemaphoreHandle_t studioLightMode_Sem_H = NULL;

EXT_RAM_BSS_ATTR TaskHandle_t studioLight_Task_H = NULL;
void studioLight_App_Task(void *);
// supporting functions

// button and encoder functions
void selectStudioLightColorButton(button_event_t button_Data);

// bluetooth functions
void setStudioLightColors(JsonDocument&);
void setScreenBrightness(JsonDocument&);
void setStudioLightMode(JsonDocument&);
void setStudioLightDuration(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *studioLight_App = new Mtb_Applications_FullScreen(studioLight_App_Task, &studioLight_Task_H, "studioLight", 4096);

void studioLight_App_Task(void* dApplication){
  Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
  THIS_APP->mtb_App_Set_EC11_Cb_Fns(selectStudioLightColorButton, mtb_Brightness_Control);
  THIS_APP->mtb_App_Set_Ble_Comm_Sv_Fns(setStudioLightColors, setScreenBrightness, setStudioLightMode, setStudioLightDuration);
  THIS_APP->mtb_App_Init();
  //************************************************************************************ */
    if(studioLightMode_Sem_H == NULL) studioLightMode_Sem_H = xSemaphoreCreateBinary();

    studioLightsInfo = (StudioLight_Data_t){
        {WHITE, PURPLE, GREEN, YELLOW, BLUE},
        FULLSCREEN_MODE,
        100};

    mtb_Read_Nvs_Struct("studioLight", &studioLightsInfo, sizeof(StudioLight_Data_t));

    xSemaphoreGive(studioLightMode_Sem_H);      // This line of code is needed to make the loop below execute once for the first time.

    uint16_t cycleClockPeriodCounter = 0;

    while (THIS_APP_IS_ACTIVE == pdTRUE){
        if(xSemaphoreTake(studioLightMode_Sem_H, pdMS_TO_TICKS(100)) == pdTRUE || studioLightsInfo.studioLightColorMode == CYCLE_MODE){
            if(studioLightsInfo.studioLightColorMode == FULLSCREEN_MODE){
                mtb_Panel_Fill_Screen(studioLightsInfo.studioLightColor[0]);
            }else if (studioLightsInfo.studioLightColorMode == HALFSCREEN_MODE){
                mtb_Panel_Fill_Rect(0, 0, 63, 63, studioLightsInfo.studioLightColor[0]);
                mtb_Panel_Fill_Rect(63, 0, 127, 63, studioLightsInfo.studioLightColor[1]);
            }else if (studioLightsInfo.studioLightColorMode == CYCLE_MODE){
                cycleClockPeriodCounter = studioLightsInfo.studioLightDuration;
                static uint8_t colorIndex = 0;
                mtb_Panel_Fill_Screen(studioLightsInfo.studioLightColor[colorIndex]);
                colorIndex++;
                if(colorIndex >= 5) colorIndex = 0;
                while (cycleClockPeriodCounter-->0 && THIS_APP_IS_ACTIVE == pdTRUE && uxSemaphoreGetCount(studioLightMode_Sem_H) < 1) delay(100);
            }
        }
    }
    vSemaphoreDelete(studioLightMode_Sem_H);
    studioLightMode_Sem_H = NULL;
  mtb_Delete_This_App(THIS_APP);
}

void selectStudioLightColorButton(button_event_t button_Data){
            switch (button_Data.type){
            case BUTTON_RELEASED:
            break;

            case BUTTON_PRESSED:
            //xSemaphoreGive(studioLightMode_Sem_H);
            do_beep(CLICK_BEEP);
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

void setStudioLightColors(JsonDocument& dCommand){
    uint8_t colorIndex = dCommand["colorIndex"];
    const char* selectColor = dCommand["colorVal"];
    studioLightsInfo.studioLightColor[colorIndex] = mtb_Panel_Color32bit_To_Color565(selectColor);
    xSemaphoreGive(studioLightMode_Sem_H);
    //printf("Instruction 0 has been received.\n");
    mtb_Write_Nvs_Struct("studioLight", &studioLightsInfo, sizeof(StudioLight_Data_t));
}

void setScreenBrightness(JsonDocument& dCommand){
    uint16_t tempBrightness = dCommand["value"];
    panelBrightness = (tempBrightness * 2.55) + 1; // One (1) is added to make the 100% correspond to 255
    mtb_Panel_Set_Brightness(panelBrightness); // 0-255
    mtb_Set_Status_RGB_LED(currentStatusLEDcolor);
    //printf("Instruction 1 has been received.\n");
    mtb_Write_Nvs_Struct("pan_brghnss", &panelBrightness, sizeof(uint8_t));
}

void setStudioLightMode(JsonDocument& dCommand){
    uint8_t mode = dCommand["lightMode"];
    //printf("Instruction 2 has been received.\n");
    if(mode <= CYCLE_MODE) studioLightsInfo.studioLightColorMode = mode;
    xSemaphoreGive(studioLightMode_Sem_H);
    mtb_Write_Nvs_Struct("studioLight", &studioLightsInfo, sizeof(StudioLight_Data_t));
}

void setStudioLightDuration(JsonDocument& dCommand){
    uint16_t duration = dCommand["dInterval"];
    //printf("Instruction 3 has been received.\n");
    studioLightsInfo.studioLightDuration = duration * 10;
    xSemaphoreGive(studioLightMode_Sem_H);
    mtb_Write_Nvs_Struct("studioLight", &studioLightsInfo, sizeof(StudioLight_Data_t));
}

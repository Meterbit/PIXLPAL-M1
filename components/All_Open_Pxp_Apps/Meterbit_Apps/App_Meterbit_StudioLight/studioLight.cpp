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
#include "studioLight.h"

StudioLight_Data_t studioLightsInfo;

EXT_RAM_BSS_ATTR TaskHandle_t studioLight_Task_H = NULL;
void studioLight_App_Task(void *);
// supporting functions

// button and encoder functions
void selectStudioLightPatternButton(button_event_t button_Data);

// bluetooth functions
void setStudioLightColors(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *studioLight_App = new Mtb_Applications_FullScreen(studioLight_App_Task, &studioLight_Task_H, "studioLight", 10240);

void studioLight_App_Task(void* dApplication){
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_EncoderFn_ptr = mtb_Brightness_Control;
  thisApp->mtb_App_ButtonFn_ptr = selectStudioLightPatternButton;
  mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(setStudioLightColors);
  mtb_App_Init(thisApp);
  //************************************************************************************ */
  mtb_Read_Nvs_Struct("studioLight", &studioLightsInfo, sizeof(StudioLight_Data_t));

while (MTB_APP_IS_ACTIVE == pdTRUE) {

    while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (MTB_APP_IS_ACTIVE == pdTRUE) {}

}

  mtb_End_This_App(thisApp);
}

void selectStudioLightPatternButton(button_event_t button_Data){
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

void setStudioLightColors(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["duration"];

    mtb_Write_Nvs_Struct("studioLight", &studioLightsInfo, sizeof(StudioLight_Data_t));
    mtb_Ble_App_Cmd_Respond_Success(studioLightAppRoute, cmdNumber, pdPASS);
}
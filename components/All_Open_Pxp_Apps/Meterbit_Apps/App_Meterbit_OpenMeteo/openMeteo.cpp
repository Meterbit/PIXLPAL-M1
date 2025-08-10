#include <Arduino.h>
#include <HTTPClient.h>
#include "mtb_github.h"
#include "mtb_text_scroll.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "mtb_nvs.h"
#include "openMeteo.h"
#include "mtb_engine.h"

OpenMeteoData_t currentOpenMeteoData = {
    "Lagos, Nigeria"
};

EXT_RAM_BSS_ATTR TaskHandle_t openMeteo_Task_H = NULL;
void openMeteoUpdate_App_Task(void *);
// supporting functions

// button and encoder functions
void changeOpenMeteoLocation(button_event_t button_Data);

// bluetooth functions
void setOpenMeteoLocation(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *openMeteo_App = new Mtb_Applications_StatusBar(openMeteoUpdate_App_Task, &openMeteo_Task_H, "Open Meteo", 10240);

void openMeteoUpdate_App_Task(void* dApplication){
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_EncoderFn_ptr = mtb_Brightness_Control;
  thisApp->mtb_App_ButtonFn_ptr = changeOpenMeteoLocation;
  mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(setOpenMeteoLocation);
  mtb_App_Init(thisApp, mtb_Status_Bar_Clock_Sv);
  //************************************************************************************ */
  mtb_Read_Nvs_Struct("openMeteo", &currentOpenMeteoData, sizeof(OpenMeteoData_t));

while (MTB_APP_IS_ACTIVE == pdTRUE) {

    while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (MTB_APP_IS_ACTIVE == pdTRUE) {}

}

  mtb_End_This_App(thisApp);
}

void changeOpenMeteoLocation(button_event_t button_Data){
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

void setOpenMeteoLocation(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["location"];
    mtb_Write_Nvs_Struct("openMeteo", &currentOpenMeteoData, sizeof(OpenMeteoData_t));
    mtb_Ble_App_Cmd_Respond_Success(openMeteoAppRoute, cmdNumber, pdPASS);
}
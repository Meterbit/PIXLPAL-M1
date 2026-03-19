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


// CONSIDER IMPLEMENTING AN APP USING THE https://api.met.no/ API SERVICE PROVIDER

struct OpenMeteoData_t {
  char location[150] = {0};
};

EXT_RAM_BSS_ATTR OpenMeteoData_t currentOpenMeteoData;

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
  thisApp->mtb_App_Set_EC11_Cb_Fns(changeOpenMeteoLocation, mtb_Brightness_Control);
  thisApp->mtb_App_Set_Ble_Comm_Sv_Fns(setOpenMeteoLocation);
  thisApp->mtb_App_Init(mtb_Status_Bar_Clock_Sv);
  //************************************************************************************ */
  currentOpenMeteoData = (OpenMeteoData_t){"Lagos, Nigeria"};
  mtb_Read_Nvs_Struct("openMeteo", &currentOpenMeteoData, sizeof(OpenMeteoData_t));

while (MTB_APP_IS_ACTIVE == pdTRUE) {

    while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (MTB_APP_IS_ACTIVE == pdTRUE) {}

}

  mtb_Delete_This_App(thisApp);
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
    String location = dCommand["location"];
    mtb_Write_Nvs_Struct("openMeteo", &currentOpenMeteoData, sizeof(OpenMeteoData_t));
}
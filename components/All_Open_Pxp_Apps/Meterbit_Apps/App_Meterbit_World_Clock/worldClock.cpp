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
#include "worldClock.h"

WorldClock_Data_t worldClockCities{
    "New York",
    "London",
    "Tokyo",
    "Sydney",
    "Moscow",
    "Los Angeles"
};

EXT_RAM_BSS_ATTR TaskHandle_t worldClock_Task_H = NULL;
void worldClock_App_Task(void *);
// supporting functions

// button and encoder functions
void selectAM_PMButton(button_event_t button_Data);

// bluetooth functions
void setWorldClockCities(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *worldClock_App = new Mtb_Applications_StatusBar(worldClock_App_Task, &worldClock_Task_H, "world Clock", 10240);

void worldClock_App_Task(void* dApplication){
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_EncoderFn_ptr = mtb_Brightness_Control;
  thisApp->mtb_App_ButtonFn_ptr = selectAM_PMButton;
  mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(setWorldClockCities);
  mtb_App_Init(thisApp);
  //************************************************************************************ */
  mtb_Read_Nvs_Struct("worldClock", &worldClockCities, sizeof(WorldClock_Data_t));

while (MTB_APP_IS_ACTIVE == pdTRUE) {

    while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (MTB_APP_IS_ACTIVE == pdTRUE) {}

}

  mtb_End_This_App(thisApp);
}

void selectAM_PMButton(button_event_t button_Data){
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

void setWorldClockCities(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["duration"];

    mtb_Write_Nvs_Struct("worldClock", &worldClockCities, sizeof(WorldClock_Data_t));
    mtb_Ble_App_Cmd_Respond_Success(worldClockAppRoute, cmdNumber, pdPASS);
}
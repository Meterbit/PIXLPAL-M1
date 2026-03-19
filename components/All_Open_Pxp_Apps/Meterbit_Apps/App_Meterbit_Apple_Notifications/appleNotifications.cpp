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

struct AppleNotification_Data_t {
uint8_t notification = 0; 
};

AppleNotification_Data_t appleNotificationInfo; // REVISIT -> Move into app stack

EXT_RAM_BSS_ATTR TaskHandle_t appleNotification_Task_H = NULL;
void appleNotifications_App_Task(void *);
// supporting functions

// bluetooth functions
void cancelAppLaunch(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *apple_Notifications_App = new Mtb_Applications_StatusBar(appleNotifications_App_Task, &appleNotification_Task_H, "apple Notif", 10240);

void appleNotifications_App_Task(void* dApplication){
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_Set_Ble_Comm_Sv_Fns(cancelAppLaunch);
  thisApp->mtb_App_Init();
  //************************************************************************************ */
  mtb_Read_Nvs_Struct("appleNotif", &appleNotificationInfo, sizeof(AppleNotification_Data_t));

while (MTB_APP_IS_ACTIVE == pdTRUE) {

    while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (MTB_APP_IS_ACTIVE == pdTRUE) {}

}

  mtb_Delete_This_App(thisApp);
}


void cancelAppLaunch(JsonDocument& dCommand){
    String location = dCommand["duration"];
    mtb_Write_Nvs_Struct("appleNotif", &appleNotificationInfo, sizeof(AppleNotification_Data_t));
}
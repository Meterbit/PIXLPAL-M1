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
#include "appleNotifications.h"

AppleNotification_Data_t appleNotificationInfo;

EXT_RAM_BSS_ATTR TaskHandle_t appleNotification_Task_H = NULL;
void appleNotifications_App_Task(void *);
// supporting functions

// bluetooth functions
void cancelAppLaunch(JsonDocument&);

EXT_RAM_BSS_ATTR Applications_StatusBar *apple_Notifications_App = new Applications_StatusBar(appleNotifications_App_Task, &appleNotification_Task_H, "apple Notif", 10240);

void appleNotifications_App_Task(void* dApplication){
  Applications *thisApp = (Applications *)dApplication;
  ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(cancelAppLaunch);
  appsInitialization(thisApp);
  //************************************************************************************ */
  read_struct_from_nvs("appleNotif", &appleNotificationInfo, sizeof(AppleNotification_Data_t));

while (THIS_APP_IS_ACTIVE == pdTRUE) {

    while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (THIS_APP_IS_ACTIVE == pdTRUE) {}

}

  kill_This_App(thisApp);
}


void cancelAppLaunch(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String location = dCommand["duration"];

    write_struct_to_nvs("appleNotif", &appleNotificationInfo, sizeof(AppleNotification_Data_t));
    mtb_Ble_App_Cmd_Respond_Success(studioLightAppRoute, cmdNumber, pdPASS);
}
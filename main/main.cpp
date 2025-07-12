#include "Arduino.h"
#include "systemm.h"
#include "littleFSMgr.h"
#include "mtbApps.h"
#include "Wifi_Man.h"
#include "esp_heap_caps.h"
#include "mtbUSB_OTA.h"
#include "littleFSMgr.h"
#include "mtbBLE_Control.h"
using namespace std;

static const char TAG[] = "PXP-MAIN";

extern "C" void app_main(){
    init_LittleFS_Mem();
    rotaryEncoder_Init();
    system_Init();
    initBLE_Communication();
    launchThisApp(firmwareUpdate_App);
    while(Applications::firmwareOTA_Status != pdFALSE) delay(1000);
    read_struct_from_nvs("currentApp", &currentApp, sizeof(CurrentApp_t));
    wifi_Initialize();
    generalAppLunch(currentApp);
    //launchThisApp(polygonFX_App);

   size_t free_sram = 0;

    while ((Applications::internetConnectStatus != true)){
      printf("Waiting for Internet Connection....\n");
      delay(1000);
    }

    // drawOnlinePNG({"https://media.api-sports.io//football//teams//165.png", 3, 17, 5});     // Draw the logo on the top left corner of the screen.
    // drawOnlinePNG({"https://media.api-sports.io//football//teams//166.png", 45, 17, 5});    // Draw the logo on the top right corner of the screen.
    // drawOnlineSVG({"https://raw.githubusercontent.com/joielechong/iso-country-flags-svg-collection/master/svg/country-4x3/us.svg", 0, 0, 1}); // Draw the MTB Logo on the top left corner of the screen.

    while (1){
    delay(5000);
    // // //printf("Name of Current App is:  %s \n", Applications::currentRunningApp->appName);
    // // //Get the total free size of internal SRAM
    free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    //Print the free SRAM size
    //printf("#############Free SRAM: %zu bytes\n", free_sram);
    //ESP_LOGI(TAG, "Memory: Free %dKiB Low: %dKiB\n", (int)xPortGetFreeHeapSize()/1024, (int)xPortGetMinimumEverFreeHeapSize()/1024);
     }
}
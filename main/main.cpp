#include "Arduino.h"
#include "mtb_system.h"
#include "mtb_engine.h"
#include "mtb_wifi.h"
#include "esp_heap_caps.h"
#include "mtb_ota.h"
#include "mtb_littleFs.h"
#include "mtb_ble.h"
using namespace std;

static const char TAG[] = "PXP-MAIN";


extern "C" void app_main(){
    mtb_LittleFS_Init();
    mtb_RotaryEncoder_Init();
    mtb_System_Init();
    mtb_Ble_Comm_Init();

    mtb_Launch_This_App(firmwareUpdate_App);
    while(Mtb_Applications::firmwareOTA_Status != pdFALSE) delay(1000);

    mtb_Read_Nvs_Struct("currentApp", &currentApp, sizeof(CurrentApp_t));
    mtb_Wifi_Init();
    mtb_General_App_Lunch(currentApp);
    //mtb_Launch_This_App(worldFlags_App);

   size_t free_sram = 0;

    // while ((Applications::internetConnectStatus != true)){
    //   ESP_LOGI(TAG, "Waiting for Internet Connection....\n");
    //   delay(1000);
    // }
    // drawOnlinePNG({"https://media.api-sports.io//football//teams//165.png", 3, 17, 5});     // Draw the logo on the top left corner of the screen.
    // SVG_OnlineImage_t testImage = {"https://raw.githubusercontent.com/woble/flags/refs/heads/master/SVG/3x2/in.svg", 0, 0, 1};
    // drawOnlineSVGs(&testImage); // Draw the MTB Logo on the top left corner of the screen.


    while (1){
    delay(2000);
    // // //ESP_LOGI(TAG, "Name of Current App is:  %s \n", Applications::currentRunningApp->appName);
    // // //Get the total free size of internal SRAM
    free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    //Print the free SRAM size
    ESP_LOGI(TAG, "#############Free SRAM: %zu bytes\n", free_sram);
    //ESP_LOGI(TAG, "Memory: Free %dKiB Low: %dKiB\n", (int)xPortGetFreeHeapSize()/1024, (int)xPortGetMinimumEverFreeHeapSize()/1024);
     }

}

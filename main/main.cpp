#include "Arduino.h"
#include "mtb_system.h"
#include "mtb_engine.h"
#include "mtb_wifi.h"
#include "mtb_ota.h"
#include "mtb_littleFs.h"
#include "mtb_ble.h"
#include "esp_heap_caps.h"
#include <HardwareSerial.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "my_secret_keys.h"
#include "exampleDrawShapesApp.h"

using namespace std;

// extern Mtb_Applications_FullScreen *exampleDrawOnlineImages_App;

static const char TAG[] = "PXP-MAIN_PROG";

extern "C" void app_main(){
    // Initialize Pixlpal System
    Serial.begin(115200);
    mtb_LittleFS_Init();
    mtb_RotaryEncoder_Init();
    mtb_System_Init();
    mtb_Ble_Comm_Init();
    mtb_Wifi_Init();

    // Cycling through a number of user selected apps, or Launch the Last Executed App or Launch a particular App after boot-up
    if(cycling_Apps.appsCycleActive == true) mtb_Launch_This_Service(mtb_App_Cycling_Sv);
    else mtb_Identify_App_By_ID(lastSavedApp, LAUNCH_PXP_APP);

    // mtb_Launch_This_App(exampleDrawOnlineImages_App);

    // Declare Variable for monitoring Free/Available internal SRAM
    size_t free_sram = 0;

    // While Loop prints available Internal SRAM every 2 seconds
    while (1){
        
    // Get the total free size of internal SRAM
    free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    
    // Print the free SRAM size to the console.
    // Serial.print("Free Internal SRAM: ");
    // Serial.print(free_sram);
    // Serial.println(" bytes");

    // delay 30 seconds
    delay(30000);
     }

}


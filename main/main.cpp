#include "Arduino.h"
#include "mtb_system.h"
#include "mtb_engine.h"
#include "mtb_wifi.h"
#include "mtb_ota.h"
#include "mtb_littleFs.h"
#include "mtb_ble.h"
#include "esp_heap_caps.h"
#include <HardwareSerial.h>

using namespace std;

static const char TAG[] = "PXP-MAIN_PROG";

extern "C" void app_main(){
    // Initialize Pixlpal System
    Serial.begin(115200); 
    mtb_LittleFS_Init();
    mtb_RotaryEncoder_Init();
    mtb_System_Init();
    mtb_Ble_Comm_Init();
    mtb_Wifi_Init();

    // Launch the Last Executed App or Launch a particular App after boot-up
    
    // if(cycling_Apps.appsShouldCycle == true) mtb_Launch_This_Service(mtb_App_Cycling_Sv);
    // else mtb_General_App_Register(activateUserApp);
    mtb_Launch_This_App(worldFlags_App);

    // Declare Variable for monitoring Free/Available internal SRAM
    size_t free_sram = 0;


    printf("The size of mtb_BleCom_Data_Trans_t is: %zu bytes\n", sizeof(mtb_BleCom_Data_Trans_t));

    // While Loop prints available Internal SRAM every 2 seconds
    while (1){
        
    // Get the total free size of internal SRAM
    free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    
    // Print the free SRAM size to the console.
    printf("############ Free Internal SRAM: %zu bytes\n", free_sram);

    // delay 5 seconds
    delay(5000);
     }
}


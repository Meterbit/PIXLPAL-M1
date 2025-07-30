#include "Arduino.h"
#include "systemm.h"
#include "littleFSMgr.h"
#include "mtbApps.h"
#include "Wifi_Man.h"
#include "esp_heap_caps.h"
#include "mtbUSB_OTA.h"
#include "littleFSMgr.h"
#include "mtbBLE_Control.h"
#include "USBMSCFS.h"
#include "beep.h"
using namespace std;

static const char TAG[] = "PXP-MAIN";

// Helper: write a text buffer to a file on the USB drive.
static void write_text_file(const char* rel_path, const char* text) {
  FILE* f = usbmscfs::open(rel_path, "w");   // path is relative to /usb
  if (!f) {
    printf("open(%s) failed\n", rel_path);
    return;
  }
  size_t n = fwrite(text, 1, strlen(text), f);
  fflush(f);
  fsync(fileno(f));                           // ensure data is flushed to the device
  fclose(f);
  printf("Wrote %u bytes to /usb/%s\n", (unsigned)n, rel_path);
}

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
    //launchThisApp(worldFlags_App);

   size_t free_sram = 0;

    // while ((Applications::internetConnectStatus != true)){
    //   printf("Waiting for Internet Connection....\n");
    //   delay(1000);
    // }

    // drawOnlinePNG({"https://media.api-sports.io//football//teams//165.png", 3, 17, 5});     // Draw the logo on the top left corner of the screen.
    // drawOnlinePNG({"https://media.api-sports.io//football//teams//166.png", 45, 17, 5});    // Draw the logo on the top right corner of the screen.
    //SVG_OnlineImage_t testImage = {"https://raw.githubusercontent.com/woble/flags/refs/heads/master/SVG/3x2/in.svg", 0, 0, 1};
    //printf("Current Flag: %s\n", testImage.imageLink); 
    //drawOnlineSVGs(&testImage); // Draw the MTB Logo on the top left corner of the screen.


    // while (1){
    // delay(2000);
    // // // //printf("Name of Current App is:  %s \n", Applications::currentRunningApp->appName);
    // // // //Get the total free size of internal SRAM
    // free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // //Print the free SRAM size
    // //printf("#############Free SRAM: %zu bytes\n", free_sram);
    // //ESP_LOGI(TAG, "Memory: Free %dKiB Low: %dKiB\n", (int)xPortGetFreeHeapSize()/1024, (int)xPortGetMinimumEverFreeHeapSize()/1024);
    //  }

//     printf("\nUSB MSC demo: create and read a .txt file\n");

//   // Mount USB mass storage at /usb (this waits for the device to be connected)
//   esp_err_t err = usbmscfs::begin("/usb");
//   printf("usbmscfs::begin -> %s\n", esp_err_to_name(err));
//   if (err != ESP_OK) {
//     do_beep(CLICK_BEEP);
//     printf("Failed to initialize USB MSC. Check power/cabling and try again.\n");
//     return;
//   }

//   // Ensure a folder exists
//   if (!usbmscfs::exists("log")) {
//     if (usbmscfs::make_dir("log")) {
//       printf("Created /usb/log\n");
//     } else {
//       printf("Failed to create /usb/log (it may already exist).\n");
//     }
//   }

//   // Create and write a text file
//   write_text_file("log/example.txt", "Hello from ESP32!\nThis is a test line written to USB.\n");

//   // Read it back
//   FILE* f = usbmscfs::open("log/example.txt", "r");
//   if (f) {
//     char buf[128];
//     size_t r = fread(buf, 1, sizeof(buf) - 1, f);
//     buf[r] = 0;
//     fclose(f);
//     printf("Read back (%u bytes):\n%s\n", (unsigned)r, buf);
//   } else {
//     printf("Failed to open file for reading.\n");
//   }

//   // (Optional) show capacity
//   uint64_t cap = 0;
//   if (usbmscfs::get_capacity(&cap) == ESP_OK) {
//     printf("Drive capacity: %llu bytes\n", (unsigned long long)cap);
//   }

//   while(1) delay(1000);  // Keep the app running to allow USB MSC operations
}
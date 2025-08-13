# PIXLPAL - A project by [Meterbit Cybernetics](https://meterbitcyb.com)

## Pixlpal Firmware
Pixlpal is a smart AIoT desktop companion with an interactive LED display. This Pixlpal-M1 firmware is an ESP-IDF Project consisting of a collection of ESP‑IDF/Arduino components and applications designed for the 128×64 RGB LED matrix display and targeting the Esp32s3 SoC. Each application runs as a standalone task that can be launched from the Android or iOS mobile app - "Pixlpal" via BLE commands. The project demonstrates how to build network enabled visual applications such as clocks, calendars, stocks and crypto tickers, AI-powered conversational assistant and various API integrations for the PIXLPAL hardware platform.

<p align="center">
  <img src="https://github.com/Meterbit/PIXLPAL-M1/blob/2076007f27073ba415204921fb2bb9618e2c804c/Pixpal-Github.jpeg" alt="Cover Image">
</p>

## Features

- Modular application framework using FreeRTOS tasks
- Targeting the ESP32-S3 which provides acceleration for neural network computing and signal processing workloads.
- Integrate any Arduino Library or ESP Component from the esp component registry
- Write texts and draw shapes, gif, png or svg images on the 128x64 RGB Matrix Display
- Wi‑Fi & BLE connectivity for internet access and remote control/configuration respectively
- Audio processing components - Microphone audio in, and DAC audio out via I2S (ESP32-audioI2S by schreibfaul1)
- Available USB-OTG/CDC component for read/write of usb flash drives.
- Dual OTA modes (i) Github releases (ii) USB Drive offline OTA Update
- Multi-functional Rotary encoder component for offline control (ebtn by saawsm)
- MQTT supported using the PicoMQTT Library by Mlesniew

## Limitations

- Requires PSRAM (Project tested on the ESP32-S3 SoC Module with 16MB flash and 8MB PSRAM - N16R8 variant)
- No Bluetooth Classic Capabilities (ESP32-S# features Bluetooth LE 5.0 only) - Use externa Bluetooth transmitter module for wireless audio transmission
- No batteries management system (The PIXLPAL-M1 has no internal batteries)

## To-Dos

- Implement vertical text scroll
- Implement jpg or jpeg decoder
- Integrate esp-nn library (espressif official Neural Network library) for AI applications
- Implement external speakers and mice support
- Complete apps currently in development - iOS notifications, Google and Outlook Calendars, News RSS, OpenWeather and World Clock Apps.
- Implement Bluetooth LE Central mode for wireless sensor interfacing 

## Quick Start
This project uses the ESP‑IDF build system with the Arduino framework as a component. To build you need the ESP‑IDF v5.3.2 release with its tools installed on your MAC or Windows PC (Install on Microsoft VS Code).
Follow [this instruction](https://github.com/espressif/vscode-esp-idf-extension) provided by espressif to install the ESP-IDF on your MAC/Windows. Windows users can follow the instructions on [this video](https://www.youtube.com/watch?v=D0fRc4XHBNk).  
Clone/download this repo to a folder in your computer. While in VSCode window, click File>Open Folder>  and locate the folder containing the Pixlpal-M1 project. When the project folder is opened on your machine, attempt to compile the project by clicking on the build button (spanner) at the bottom of your Vscode IDE window.


### Main.cpp - Program Execution StartPoint
```c++
#include "Arduino.h"
#include "mtb_system.h"
#include "mtb_engine.h"
#include "mtb_wifi.h"
#include "mtb_ota.h"
#include "mtb_littleFs.h"
#include "mtb_ble.h"
#include "esp_heap_caps.h"
using namespace std;

static const char TAG[] = "PXP-MAIN PROG";

extern "C" void app_main(){
    // Initialize Pixlpal System
    mtb_LittleFS_Init();
    mtb_RotaryEncoder_Init();
    mtb_System_Init();
    mtb_Ble_Comm_Init();

    // Attempt OTA Update from USB Flash Drive
    mtb_Launch_This_App(firmwareUpdate_App);
    while(Mtb_Applications::firmwareOTA_Status != pdFALSE) delay(1000);

    // Read the last executed App from NVS
    mtb_Read_Nvs_Struct("currentApp", &currentApp, sizeof(Mtb_CurrentApp_t));

    // Initialize Wifi
    mtb_Wifi_Init();

    // Launch the Last Executed App or Launch a particular App after boot-up
    // mtb_General_App_Lunch(currentApp);
    mtb_Launch_This_App(exampleWriteTextApp);

    // Declare Variable for monitoring Free/Available internal SRAM
    size_t free_sram = 0;

    // While Loop prints available Internal SRAM every 2 seconds
    while (1){
    // Get the total free size of internal SRAM
    free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    // Print the free SRAM size
    ESP_LOGI(TAG, "############ Free Internal SRAM: %zu bytes\n", free_sram);
    //ESP_LOGI(TAG, "Memory: Free %dKiB Low: %dKiB\n", (int)xPortGetFreeHeapSize()/1024, (int)xPortGetMinimumEverFreeHeapSize()/1024);

    // delay 2 seconds
    delay(2000);
     }

}
```

### Example Application - Write_Text_App
````c++
#include <Arduino.h>
#include "mtb_text_scroll.h"
#include "mtb_engine.h"
#include "mtb_buzzer.h"
#include "exampleWriteTextApp.h"

ExampleApp_Data_t exampleAppInfo;

EXT_RAM_BSS_ATTR TaskHandle_t exampleApp_Task_H = NULL;

// button and encoder functions
void exampleAppButton(button_event_t button_Data);
void exampleAppEncoder(rotary_encoder_rotation_t direction);

// bluetooth functions
void exampleAppBleComTest(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *exampleWriteTextApp = new Mtb_Applications_FullScreen(exampleApp_Task, &exampleApp_Task_H, "exampleWriteTextApp", 4096);

void exampleApp_Task(void* dApplication){
// ****** Initialize the App Parameters
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_EncoderFn_ptr = exampleAppEncoder;
  thisApp->mtb_App_ButtonFn_ptr = exampleAppButton;
  //mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(exampleAppBleComTest);
  mtb_App_Init(thisApp);
// End of App parameter initialization

// ****** Recover Saved App settings from NVS
// mtb_Write_Nvs_Struct("exampleAppData", &exampleAppInfo, sizeof(exampleAppInfo));

// Declare Fixed and Scroll Text Variables
Mtb_FixedText_t exampleFixedText(24,15, Terminal8x12, GREEN);
Mtb_ScrollText_t exampleScrollText (5, 40, 118, WHITE, 20, 1, Terminal6x8);

// Write Fixed Text to display
exampleFixedText.mtb_Write_String("Hello World");

while (MTB_APP_IS_ACTIVE == pdTRUE) {
// Scroll the ScrollText Variable on display every 15 seconds
exampleScrollText.mtb_Scroll_This_Text("PIXLPAL - A project by Meterbit Cybernetics");
delay(15000);
}

// Clean up the application before exiting
  mtb_End_This_App(thisApp);
}

// ROTARY ENCODER CALLBACK FUNCTION
void exampleAppEncoder(rotary_encoder_rotation_t direction){
    if (direction == ROT_CLOCKWISE){
    do_beep(CLICK_BEEP);
    } else if(direction == ROT_COUNTERCLOCKWISE){
    do_beep(CLICK_BEEP);
    }
}


// BUTTON CALLBACK FUNCTION
void exampleAppButton(button_event_t button_Data){
            switch (button_Data.type){
            case BUTTON_RELEASED:
            do_beep(BEEP_0);
            break;

            case BUTTON_PRESSED:
            do_beep(BEEP_0);
            break;

            case BUTTON_PRESSED_LONG:
            do_beep(BEEP_0);
            break;

            case BUTTON_CLICKED:
            //ESP_LOGI(TAG, "Button Clicked: %d Times\n",button_Data.count);
            switch (button_Data.count){
            case 1:
                break;
            case 2:
            do_beep(BEEP_0);
                break;
            case 3:
            do_beep(BEEP_0);
                break;
            default:
                break;
            }
                break;
            default:
            break;
			}
}


// BLE COMMUNICATION CALLBACK FUNCTION
// This function will be called when the application receives a command from the BLE client.
// void exampleAppBleComTest(JsonDocument& dCommand){
//     uint8_t cmdNumber = dCommand["app_command"];
//     String exampleCmd = dCommand["expl_Cmd"];

//     mtb_Write_Nvs_Struct("exampleAppData", &exampleAppInfo, sizeof(exampleAppInfo));
//     mtb_Ble_App_Cmd_Respond_Success(exampleAppRouteRoute, cmdNumber, pdPASS);
// }
````

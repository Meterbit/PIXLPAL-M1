# PIXLPAL – A Project by [Meterbit Cybernetics](https://meterbitcyb.com)

## Overview
Pixlpal is a smart AIoT desktop companion featuring an interactive 128×64 RGB LED matrix display.  
The **Pixlpal-M1** firmware is built on the **ESP-IDF** framework, combining ESP-IDF components, Arduino libraries, and custom applications for the ESP32-S3 SoC.  

Each application runs as a standalone **FreeRTOS** task and can be launched via BLE commands from the Pixlpal Android or iOS mobile app.  
The firmware demonstrates how to build network-enabled visual applications such as clocks, calendars, stock/crypto tickers, AI-powered conversational assistants, and various API integrations.

<p align="center">
  <img src="https://github.com/Meterbit/PIXLPAL-M1/blob/2076007f27073ba415204921fb2bb9618e2c804c/Pixpal-Github.jpeg" alt="Pixlpal Cover Image">
</p>

---

## Key Features
- **Modular Application Framework** – Built on FreeRTOS tasks for easy expansion.
- **ESP32-S3 Target** – Optimized for neural network acceleration and signal processing.
- **Flexible Library Integration** – Supports any Arduino library or ESP-IDF component.
- **Rich Graphics Capabilities** – Draw text, shapes, GIF, PNG, and SVG images.
- **Connectivity** – Wi-Fi for internet access, BLE for remote control and configuration.
- **Audio Processing** – Microphone input and DAC audio output via I2S (`ESP32-audioI2S` by schreibfaul1).
- **USB-OTG/CDC** – Read/write USB flash drives.
- **Dual OTA Update Modes**:  
  1. From GitHub releases  
  2. From USB flash drive (offline)
- **Rotary Encoder Control** – Multi-functional input (`ebtn` by saawsm).
- **MQTT Support** – Integrated via the `PicoMQTT` library by Mlesniew.

---

## Limitations
- **Requires PSRAM** – Tested on the ESP32-S3 (N16R8: 16 MB flash, 8 MB PSRAM).
- **No Bluetooth Classic** – BLE 5.0 only; use an external Bluetooth transmitter for wireless audio.
- **No Battery Management** – No onboard battery system.

---

## Planned Features / To-Do
- Vertical text scrolling.
- JPEG/JPG decoder implementation.
- Integration of `esp-nn` (Espressif’s official neural network library).
- External speaker and mouse support.
- Completion of in-progress apps:  
  iOS Notifications, Google & Outlook Calendars, News RSS, OpenWeather, and World Clock.
- BLE Central mode for wireless sensor integration.
- Addition of BOM to hardware design files.

---

## Quick Start

**Build Requirements:**
- **ESP-IDF v5.3.2** installed on macOS or Windows (via VS Code).  
  - [ESP-IDF VS Code Installation Guide](https://github.com/espressif/vscode-esp-idf-extension)  
  - [Windows Installation Video](https://www.youtube.com/watch?v=D0fRc4XHBNk)  

**Steps:**
1. Clone or download this repository.
2. In VS Code: **File → Open Folder** → select the Pixlpal-M1 project folder.
3. Build the project by clicking the **build icon (spanner)** in the bottom status bar.

---

## Main Program Entry Point
**`main.cpp` – Application Startup Flow**
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

extern "C" void app_main() {
    mtb_LittleFS_Init();
    mtb_RotaryEncoder_Init();
    mtb_System_Init();
    mtb_Ble_Comm_Init();

    mtb_Launch_This_App(firmwareUpdate_App);
    while (Mtb_Applications::firmwareOTA_Status != pdFALSE) delay(1000);

    mtb_Read_Nvs_Struct("currentApp", &currentApp, sizeof(Mtb_CurrentApp_t));

    mtb_Wifi_Init();

    mtb_Launch_This_App(exampleWriteTextApp);

    size_t free_sram = 0;
    while (1) {
        free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "############ Free Internal SRAM: %zu bytes\n", free_sram);
        delay(2000);
    }
}

```

### Example Application
**`exampleWriteTextApp.cpp` – Displaying Fixed & Scrolling Text**
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

## Control Circuit Schematic
<p align="center">
  <img src="https://github.com/Meterbit/PIXLPAL-M1/blob/af65b201861f3fa28d05fecc5947d0f03bc81e3b/Pixlpal%20Schematic%20-%20BOM%20-%20Enclosure%20files/Pixlpal-M1%20Schematic.png" alt="Pixlpal Control Schematic">
</p>

## Licenses

- Hardware Design Licensed under the [CERN-OHL-W](https://gitlab.com/ohwr/project/cernohl/-/wikis/uploads/f773df342791cc55b35ac4f907c78602/cern_ohl_w_v2.pdf)
- Software/Firmware Design Licensed under the [LGPL v3.0](https://www.gnu.org/licenses/lgpl-3.0.en.html?utm_source=chatgpt.com#license-text)

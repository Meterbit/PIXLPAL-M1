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
- **USB-OTG/CDC** – Read/write USB flash drives, add input and output devices e.g. usb mouse, speakers etc.
- **Dual OTA Update Modes**:  
  1. From GitHub releases (online/remote) 
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
  iOS Notifications, Google & Outlook Calendars, News RSS, OpenWeather, and Spotify updates.
- BLE Central mode for wireless sensor integration.
- Addition of BOM to hardware design files.
- Integration of Home Assistant using MQTT
- Integration of ESP Matter SDK 

---

## Quick Start

**Build Requirements:**
- **ESP-IDF v5.5.X** installed on macOS or Windows (via VS Code).  
  - [Complete step-by-step installation guide for Windows, MacOS X, Linux Ubuntu](https://randomnerdtutorials.com/programming-esp32-esp-idf-vs-code/)
  - [ESP-IDF VS-Code Installation Guide](https://github.com/espressif/vscode-esp-idf-extension)  
  - [Windows Installation Video](https://www.youtube.com/watch?v=D0fRc4XHBNk)  

**Steps:**
1. Clone or download this repository.
2. In VS Code: **File → Open Folder** → select the Pixlpal-M1 project folder.
3. Build the project by clicking the **build icon (spanner)** in the bottom status bar.




**Learn FreeRTOS:**

Pixlpal-M1 runs on FreeRTOS. If you’ve used Arduino before but haven’t worked with an RTOS, we recommend getting familiar with FreeRTOS to make the most of this project.
Learn FreeRTOS by watching [this tutorial](https://www.youtube.com/watch?v=F321087yYy4&list=PLXyB2ILBXW5FLc7j2hLcX6sAGbmH0JxX8) video created by Digikey.
You only need the Tutorials 1 to 8 to work with the Pixlpal-M1 project code.

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
#include "exampleEncoderBeepApp.h"
using namespace std;

static const char TAG[] = "PXP-MAIN_PROG";

extern "C" void app_main(){
    // Initialize Pixlpal System
    mtb_LittleFS_Init();
    mtb_RotaryEncoder_Init();
    mtb_System_Init();
    mtb_Ble_Comm_Init();

    // Attempt OTA Update from USB Flash Drive
    mtb_Launch_This_App(usbOTA_Update_App);
    while(Mtb_Applications::firmwareOTA_Status != pdFALSE) delay(1000);

    // Read the last executed App from NVS
    mtb_Read_Nvs_Struct("currentApp", &currentApp, sizeof(Mtb_CurrentApp_t));

    // Initialize Wifi
    mtb_Wifi_Init();

    // Launch the Last Executed App or Launch a particular App after boot-up
    // mtb_General_App_Lunch(currentApp);
    mtb_Launch_This_App(exampleWriteText_App);

    // Declare Variable for monitoring Free/Available internal SRAM
    size_t free_sram = 0;

    // While Loop prints available Internal SRAM every 2 seconds
    while (1){

    // Get the total free size of internal SRAM
    free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    // Print the free SRAM size
    ESP_LOGI(TAG, "############ Free Internal SRAM: %zu bytes\n", free_sram);

    // delay 2 seconds
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
#include "exampleWriteTextApp.h"

EXT_RAM_BSS_ATTR TaskHandle_t exampleWriteTextApp_Task_H = NULL;

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *exampleWriteText_App = new Mtb_Applications_FullScreen(exampleWriteTextApp_Task, &exampleWriteTextApp_Task_H, "exampleWriteTextApp", 4096);

void exampleWriteTextApp_Task(void* dApplication){
// ****** Initialize the App Parameters
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  mtb_App_Init(thisApp);

// Declare Fixed and Scroll Text Objects
  Mtb_FixedText_t exampleFixedText(24, 15, Terminal8x12, GREEN);
  Mtb_ScrollText_t exampleScrollText(5, 40, 118, Terminal6x8, WHITE, 20, 1);

// Write Fixed Text to display
  exampleFixedText.mtb_Write_String("Hello World.");                     // Write text in default color
  //exampleFixedText.mtb_Write_Colored_String(" in Color!", PURPLE);     // Write text in different color

  while (MTB_APP_IS_ACTIVE == pdTRUE) {

// Scroll the ScrollText Variable on display every 15 seconds
  exampleScrollText.mtb_Scroll_This_Text("PIXLPAL - A project by Meterbit Cybernetics");      // Scroll text in default color
  //exampleScrollText.mtb_Scroll_This_Text("Visit us at www.meterbitcyb.com", CYAN);          // Scroll text in different color
  
  delay(15000);
}

// Clean up the application before exiting
  mtb_End_This_App(thisApp);
}
````

## Control Circuit Schematic
<p align="center">
  <img src="https://github.com/Meterbit/PIXLPAL-M1/blob/af65b201861f3fa28d05fecc5947d0f03bc81e3b/Pixlpal%20Schematic%20-%20BOM%20-%20Enclosure%20files/Pixlpal-M1%20Schematic.png" alt="Pixlpal Control Schematic">
</p>

## Support Pixlpal Project

We’re excited to officially announce that the Pixlpal AIoT Display has launched on Crowd Supply!

Pixlpal is now live on the crowdfunding platform, and you can support the project by becoming a backer. If you’d like to be part of this journey and help bring Pixlpal to more homes, workspaces, and maker communities, simply visit the campaign page here:

👉 https://www.crowdsupply.com/meterbit-cybernetics/pixlpal

Thank you for your support and for being part of the Pixlpal story! 🚀

## Licenses

- Hardware Design Licensed under the [CERN-OHL-W](https://gitlab.com/ohwr/project/cernohl/-/wikis/uploads/f773df342791cc55b35ac4f907c78602/cern_ohl_w_v2.pdf)
- Software/Firmware Design Licensed under the [LGPL v3.0](https://www.gnu.org/licenses/lgpl-3.0.en.html?utm_source=chatgpt.com#license-text)

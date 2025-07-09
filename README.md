# PIXLPAL-M1

PIXLPAL‑M1 is a collection of ESP‑IDF/Arduino applications designed for a 64×32 RGB LED matrix display. Each application runs as a standalone task that can be launched from a simple menu system or switched via BLE commands. The project demonstrates how to build network enabled visual gadgets such as clocks, news tickers and various API integrations for the PIXLPAL hardware platform.

## Features

- Modular application framework using FreeRTOS tasks
- BLE command parser for remote control and configuration
- Wi‑Fi connectivity with optional over‑the‑air updates
- Persistent storage via NVS and LittleFS
- Example apps for weather, crypto prices, calendar events and more

## Repository Layout

```
.
├── components/        # All application and library components
├── main/              # Project entry point
├── partition_table/   # Flash layout
├── sdkconfig          # ESP‑IDF build configuration
└── README.md          # Project documentation
```

Applications live under `components/App_*` and are registered with the framework in `main/main.cpp`. Each app exposes BLE commands for runtime configuration and stores its settings in NVS. Resources such as icons or GIF animations are served from the LittleFS filesystem.

## Building

This project uses the ESP‑IDF build system with the Arduino framework as a component. To build you need a recent ESP‑IDF release with its tools installed:

```bash
cd PIXLPAL-M1
idf.py set-target esp32
idf.py menuconfig   # optional project configuration
idf.py build
```

Connect your PIXLPAL‑M1 board and run:

```bash
idf.py -p PORT flash monitor
```

Replace `PORT` with the serial port of your device.

## BLE Control

The firmware exposes a set of BLE characteristics allowing a mobile app to switch between applications and change their settings. Each app documents the commands it accepts in its own README. Most settings are saved to NVS so they persist across reboots.


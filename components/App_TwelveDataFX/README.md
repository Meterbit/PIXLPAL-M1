# TwelveData FX App

This application displays currency exchange rates from the [TwelveData](https://twelvedata.com/) API on the PIXLPAL matrix display. The app is built as a status bar application so it runs alongside the system clock.

## BLE Commands

The app listens on BLE route `4/3` and accepts the following commands:

| Field       | Description                          |
|-------------|--------------------------------------|
| `pair`      | Currency pair to request (e.g. `EUR/USD`) |
| `dInterval` | Update interval in seconds           |
| `api_key`   | TwelveData API key                   |

Send a JSON object containing the desired fields and `app_command` number. The provided values are stored in NVS and persist across restarts.

## Persistent Settings

```cpp
struct TwelveDataFX_t {
    char pair[20];       // Currency pair
    char apiToken[100];  // API key
    int16_t updateInterval; // Refresh interval in seconds
};
```

Settings are loaded from NVS namespace `twelveDataFX` at start up. If no API key is stored the HTTP request will fail until you provide one over BLE.

## Network Security

`WiFiClientSecure` is used to connect to the TwelveData HTTPS endpoint. The project currently calls `setInsecure()` which disables certificate validation. Replace this with a call to `esp_crt_bundle_attach` or provide a CA certificate if you need strict TLS checking.

## Files

- `twelveDataFX.cpp` – main application logic
- `include/twelveDataFX.h` – data structures and task prototype
- `CMakeLists.txt` – component registration



/**
 * @file mtb_nvs.h
 * @brief Non-volatile storage (NVS) initialisation and persistent settings types.
 *
 * Wraps the ESP-IDF NVS API to provide type-safe persistent storage for device
 * configuration. The NVS access task (declared in mtb_engine.h as `nvsAccessTask`)
 * serialises all reads/writes; use `mtb_Read_Nvs_Struct` / `mtb_Write_Nvs_Struct`
 * (declared in mtb_engine.h) to access the store from any task.
 *
 * Key value types stored in NVS:
 *   - Wi-Fi credentials (`Wifi_Credentials`)
 *   - Status-bar clock colour scheme (`Clock_Colors`)
 *   - Device serial number, BLE advertising name, NTP timezone, volume level
 */
#ifndef NVS_MEM
#define NVS_MEM

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mtb_engine.h"

/**
 * @brief Wi-Fi network credentials stored in NVS.
 */
struct Wifi_Credentials {
    char ssid[32];   /**< Wi-Fi SSID (null-terminated, max 31 chars). */
    char pass[64];   /**< Wi-Fi passphrase (null-terminated, max 63 chars). */
    char ipAdr[20];  /**< Assigned IP address string (dotted-decimal notation). */
};

/**
 * @brief Colour scheme for the status-bar clock and date widgets.
 *
 * Written to NVS under the key "Clock Cols" and reloaded on each
 * `mtb_StatusBar_Clock_Task` start.
 */
struct Clock_Colors {
    uint16_t hourMinColour;  /**< Colour of the HH:MM display (RGB565). */
    uint16_t secColor;       /**< Colour of the seconds display (RGB565). */
    uint16_t meridiemColor;  /**< Colour of the AM/PM indicator (RGB565). */
    uint16_t weekDayColour;  /**< Colour of the weekday name (RGB565). */
    uint16_t dateColour;     /**< Colour of the MMM DD date string (RGB565). */
};

extern char          serial_No[20];    /**< Device serial number string (read from NVS at boot). */
extern char          pxp_BLE_Name[20]; /**< BLE advertising name (read from NVS at boot). */
extern char          ntp_TimeZone[100];/**< POSIX TZ string for the NTP time zone (read from NVS). */
extern uint8_t       deviceVolume;     /**< Audio output volume level 0–100 (read from NVS). */
extern uint8_t       litFS_Ready;      /**< pdTRUE once LittleFS is mounted and ready. */
extern Clock_Colors  clk_Updt;         /**< Live clock colour scheme, updated by BLE settings handler. */

/**
 * @brief Initialise the NVS flash partition and open the default namespace.
 * @note  Must be called once before any other NVS operation.
 */
extern void init_nvs_mem(void);

/**
 * @brief Write factory-default values to NVS (called on first boot or NVS erase).
 */
void set_factory_NVS_parameters(void);

#endif

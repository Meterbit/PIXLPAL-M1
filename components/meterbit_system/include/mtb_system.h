/**
 * @file mtb_system.h
 * @brief System power management and hardware initialisation API.
 *
 * Provides the `power_States_t` enum and the three system-level functions:
 *   - mtb_System_Init()       — full system peripheral and subsystem boot sequence
 *   - device_SD_RS()          — controlled device shutdown or restart
 *   - mtb_RotaryEncoder_Init() — rotary encoder GPIO and interrupt setup
 *
 * The global `wake` variable reflects the last programmed power transition intent
 * and is checked by the deep-sleep wake handler to determine whether to resume or
 * perform a factory restart.
 */
#ifndef __POWER_H__
#define __POWER_H__

#include "mtb_text_scroll.h"

/**
 * @brief Device power transition states.
 */
typedef enum {
    TEMP_SH  = 1, /**< Temporary shutdown: enter deep sleep, wake on timer. */
    PERM_SH,      /**< Permanent shutdown: enter deep sleep without a wake timer. */
    TEMP_RS,      /**< Temporary restart: reboot the MCU after a short delay. */
    STAY_ON       /**< Normal operation: device remains powered and running. */
} power_States_t;

extern power_States_t wake; /**< Current power-state intent; written before any shutdown/restart call. */

/**
 * @brief Perform the full system boot sequence.
 *
 * Initialises all hardware peripherals and firmware subsystems in the correct
 * dependency order: NVS, LittleFS, WiFi, BLE, graphics driver, status bar,
 * scroll tasks, MQTT, OTA, and USB.
 */
extern void mtb_System_Init(void);

/**
 * @brief Trigger a device shutdown or restart.
 *
 * Shuts down or restarts the device according to @p state. The function
 * updates the `wake` global, flushes pending PSRAM writes, and calls the
 * appropriate ESP-IDF deep-sleep or restart API.
 *
 * @param state  Power transition to perform (TEMP_SH, PERM_SH, TEMP_RS, or STAY_ON).
 */
extern void device_SD_RS(power_States_t state);

/**
 * @brief Initialise the rotary encoder GPIO pins and ISR.
 *
 * Configures the two encoder quadrature input pins (CLK, DT) and the push-button
 * pin as GPIO inputs with interrupts, and registers the encoder ISR handler used
 * by the app launcher to scroll through the app list.
 */
extern void mtb_RotaryEncoder_Init(void);

#endif  //__POWER_H__

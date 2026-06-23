/**
 * @file mtb_wifi.h
 * @brief Wi-Fi connection management API for the PIXLPAL-M1.
 *
 * Provides functions and state for initialising the Wi-Fi driver, connecting to
 * saved networks, and querying the currently connected network. Credentials are
 * persisted in NVS and restored across reboots.
 */
#ifndef meterbit_wifi_manager_H
#define meterbit_wifi_manager_H

// #ifdef __cplusplus
// extern "C" {
// #endif

#include "Arduino.h"
#include "WiFi.h"
//#include "tcpip_adapter.h"

//extern bool wifiConActive;

extern struct Wifi_Credentials last_Successful_Wifi; /**< Credentials of the most recently connected network, loaded from NVS on boot. */

/**
 * @brief Log and store the credentials of the currently associated Wi-Fi network.
 * @note  Call after a successful connection to persist the credentials for reconnection.
 */
extern void wifi_CurrentContdNetwork(void);

//extern void wifi_ConnectToNetwork(const char *, const char *);

/**
 * @brief Initialise the Wi-Fi subsystem and attempt to connect to the last known network.
 * @note  Reads saved credentials from NVS. If no credentials are stored the device
 *        waits for credentials to be supplied via BLE.
 */
extern void mtb_Wifi_Init();

extern TaskHandle_t wifi_Ntwks_Scan_Task_H; /**< Handle for the background Wi-Fi network scan task. */

//extern TimerHandle_t reconnect_Last_Station_H;
//extern void wifi_Scan_Task(void *args);

// #ifdef __cplusplus
// }
// #endif
#endif

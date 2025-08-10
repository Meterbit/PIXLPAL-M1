#ifndef meterbit_wifi_manager_H
#define meterbit_wifi_manager_H

// #ifdef __cplusplus
// extern "C" {
// #endif

#include "Arduino.h"
#include "WiFi.h"
//#include "tcpip_adapter.h"

//extern bool wifiConActive;
extern struct Wifi_Credentials last_Successful_Wifi;
extern void wifi_CurrentContdNetwork(void);
//extern void wifi_ConnectToNetwork(const char *, const char *);
extern void mtb_Wifi_Init();
extern TaskHandle_t wifi_Ntwks_Scan_Task_H;
//extern TimerHandle_t reconnect_Last_Station_H;
//extern void wifi_Scan_Task(void *args);

// #ifdef __cplusplus
// }
// #endif
#endif
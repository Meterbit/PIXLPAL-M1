#ifndef MTB_BLE_CTRL
#define MTB_BLE_CTRL

#include "ArduinoJson.h"
#include "Arduino.h"


// All Settings Communication Routes
static const char systeSettingsRoute[] = "1";
static const char wifiSettingsRoute[] = "2";
static const char bleSettingsRoute[] = "3";
static const char softwareUpdateRoute[] = "4";

extern DynamicJsonDocument dCommand;

struct bleCom_Data_Trans_t {
void *payload; // BLE message or payload
size_t pay_size;
};

extern QueueHandle_t setCom_queue;
extern QueueHandle_t appCom_queue;

extern bleCom_Data_Trans_t setCom_data;
extern bleCom_Data_Trans_t appCom_data;

extern TaskHandle_t ble_SetCom_Parser_Task_Handle;
extern TaskHandle_t ble_AppCom_Parser_Task_Handle;

extern void ble_Application_Command_Respond_Success(const char*, uint8_t, uint8_t);
//extern void bleRestoreTimerCallBkFn(TimerHandle_t);

extern void initBLE_Communication(void);
extern void deinitBLE_Communication(void);

extern int getIntegerAtIndex(const String &data, int index);

extern int bleSettingsComSend(const char* dSetRoute, String dMessage);
extern int bleApplicationComSend(const char* dAppRoute, String dMessage);

extern void ble_SetCom_Parse_Task(void *);
extern void ble_AppCom_Parse_Task(void *);

extern void current_Network(const char *, const char *);
//extern void powerSettings(DynamicJsonDocument&);
extern void systemSettings(DynamicJsonDocument&);
extern void wifiSettings(DynamicJsonDocument&);
extern void bleSettings(DynamicJsonDocument&);
extern void softwareUpdate(DynamicJsonDocument&);

//BLE SECTION
void current_BleDevice(const char*);
void set_PxpBleName(DynamicJsonDocument&);
void current_softwareVersion(const char*, const char*, const char*, const char*);
#endif
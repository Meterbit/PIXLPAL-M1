#ifndef MTB_BLE_OTA_H
#define MTB_BLE_OTA_H

#include <Arduino.h>
#include "NimBLEDevice.h"
#include "esp_err.h"

// OTA protocol UUIDs
#define MTB_BLE_OTA_SERVICE_UUID   "487d0950-41b9-4c57-ad09-a46ac47e2150"
#define MTB_BLE_OTA_CTRL_UUID      "487d0950-41b9-4c57-ad09-a46ac47e2151"
#define MTB_BLE_OTA_DATA_UUID      "487d0950-41b9-4c57-ad09-a46ac47e2152"

// Control commands from client
#define MTB_BLE_OTA_CMD_START      0x01
#define MTB_BLE_OTA_CMD_COMPLETE   0x02

// Notifications back to client
#define MTB_BLE_OTA_STATUS_START_ACK      0x01
#define MTB_BLE_OTA_STATUS_COMPLETE_ACK   0x02
#define MTB_BLE_OTA_STATUS_ERROR          0xFF

typedef enum {
    MTB_BLE_OTA_ERR_NONE                 = 0x00,
    MTB_BLE_OTA_ERR_INVALID_CTRL_PACKET  = 0x01,
    MTB_BLE_OTA_ERR_BAD_START_LENGTH     = 0x02,
    MTB_BLE_OTA_ERR_NO_PARTITION         = 0x03,
    MTB_BLE_OTA_ERR_BEGIN_FAILED         = 0x04,
    MTB_BLE_OTA_ERR_NOT_ACTIVE           = 0x05,
    MTB_BLE_OTA_ERR_BAD_COMPLETE_LENGTH  = 0x06,
    MTB_BLE_OTA_ERR_SIZE_MISMATCH        = 0x07,
    MTB_BLE_OTA_ERR_CRC_MISMATCH         = 0x08,
    MTB_BLE_OTA_ERR_END_FAILED           = 0x09,
    MTB_BLE_OTA_ERR_SET_BOOT_FAILED      = 0x0A,
    MTB_BLE_OTA_ERR_UNKNOWN_COMMAND      = 0x0B,
    MTB_BLE_OTA_ERR_DATA_BEFORE_START    = 0x0C,
    MTB_BLE_OTA_ERR_EMPTY_CHUNK          = 0x0D,
    MTB_BLE_OTA_ERR_WRITE_FAILED         = 0x0E,
    MTB_BLE_OTA_ERR_SERVICE_NOT_READY    = 0x0F,
} mtb_ble_ota_error_t;

bool mtb_Ble_Ota_Init(NimBLEServer *server);
void mtb_Ble_Ota_Deinit(void);

bool mtb_Ble_Ota_Is_Active(void);
bool mtb_Ble_Ota_Is_Ready(void);

void mtb_Ble_Ota_Set_Enabled(bool enable);
bool mtb_Ble_Ota_Is_Enabled(void);

void mtb_Ble_Ota_Enter_Ready_Mode(void);
void mtb_Ble_Ota_Abort(void);

uint32_t mtb_Ble_Ota_Get_Expected_Size(void);
uint32_t mtb_Ble_Ota_Get_Received_Size(void);
uint32_t mtb_Ble_Ota_Get_Running_Crc(void);

#endif // MTB_BLE_OTA_H
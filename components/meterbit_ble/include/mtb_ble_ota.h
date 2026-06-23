/**
 * @file mtb_ble_ota.h
 * @brief BLE-based OTA (Over-The-Air) firmware update service API.
 *
 * Exposes a NimBLE GATT service with two characteristics:
 *   - Control (MTB_BLE_OTA_CTRL_UUID): receives start/complete commands and sends
 *     status notifications back to the OTA client.
 *   - Data (MTB_BLE_OTA_DATA_UUID): receives raw firmware binary chunks written by
 *     the companion app during an update.
 *
 * Call mtb_Ble_Ota_Init() after the BLE server is created to register the service.
 * The update writes directly to the next OTA partition via esp_ota_ops.
 */
#ifndef MTB_BLE_OTA_H
#define MTB_BLE_OTA_H

#include <Arduino.h>
#include "NimBLEDevice.h"
#include "esp_err.h"

/** @defgroup mtb_ble_ota_uuids BLE OTA GATT UUIDs
 * @{ */
#define MTB_BLE_OTA_SERVICE_UUID   "487d0950-41b9-4c57-ad09-a46ac47e2150" /**< OTA GATT service UUID. */
#define MTB_BLE_OTA_CTRL_UUID      "487d0950-41b9-4c57-ad09-a46ac47e2151" /**< Control characteristic: receives commands, sends status notifications. */
#define MTB_BLE_OTA_DATA_UUID      "487d0950-41b9-4c57-ad09-a46ac47e2152" /**< Data characteristic: receives binary firmware chunks. */
/** @} */

/** @defgroup mtb_ble_ota_cmds BLE OTA Control Commands (client → device)
 * @{ */
#define MTB_BLE_OTA_CMD_START      0x01 /**< Begin OTA session; payload contains expected total firmware size (4 bytes LE). */
#define MTB_BLE_OTA_CMD_COMPLETE   0x02 /**< Finalise OTA; payload contains expected CRC-32 (4 bytes LE). */
/** @} */

/** @defgroup mtb_ble_ota_status BLE OTA Status Notifications (device → client)
 * @{ */
#define MTB_BLE_OTA_STATUS_START_ACK      0x01 /**< Device acknowledged the START command and is ready to receive data. */
#define MTB_BLE_OTA_STATUS_COMPLETE_ACK   0x02 /**< OTA completed successfully; device will reboot. */
#define MTB_BLE_OTA_STATUS_ERROR          0xFF /**< An error occurred; the second byte contains mtb_ble_ota_error_t code. */
/** @} */

/**
 * @brief Error codes reported in OTA status notifications.
 */
typedef enum {
    MTB_BLE_OTA_ERR_NONE                 = 0x00, /**< No error. */
    MTB_BLE_OTA_ERR_INVALID_CTRL_PACKET  = 0x01, /**< Control packet length or format invalid. */
    MTB_BLE_OTA_ERR_BAD_START_LENGTH     = 0x02, /**< START command did not contain a valid 4-byte size field. */
    MTB_BLE_OTA_ERR_NO_PARTITION         = 0x03, /**< No OTA target partition found. */
    MTB_BLE_OTA_ERR_BEGIN_FAILED         = 0x04, /**< esp_ota_begin() returned an error. */
    MTB_BLE_OTA_ERR_NOT_ACTIVE           = 0x05, /**< Data received without an active OTA session. */
    MTB_BLE_OTA_ERR_BAD_COMPLETE_LENGTH  = 0x06, /**< COMPLETE command did not contain a valid 4-byte CRC field. */
    MTB_BLE_OTA_ERR_SIZE_MISMATCH        = 0x07, /**< Received byte count does not match the expected size. */
    MTB_BLE_OTA_ERR_CRC_MISMATCH         = 0x08, /**< Computed CRC-32 does not match the client-supplied value. */
    MTB_BLE_OTA_ERR_END_FAILED           = 0x09, /**< esp_ota_end() returned an error. */
    MTB_BLE_OTA_ERR_SET_BOOT_FAILED      = 0x0A, /**< esp_ota_set_boot_partition() returned an error. */
    MTB_BLE_OTA_ERR_UNKNOWN_COMMAND      = 0x0B, /**< Unrecognised control command byte. */
    MTB_BLE_OTA_ERR_DATA_BEFORE_START    = 0x0C, /**< Data chunk received before START was acknowledged. */
    MTB_BLE_OTA_ERR_EMPTY_CHUNK          = 0x0D, /**< Zero-length data chunk received. */
    MTB_BLE_OTA_ERR_WRITE_FAILED         = 0x0E, /**< esp_ota_write() returned an error. */
    MTB_BLE_OTA_ERR_SERVICE_NOT_READY    = 0x0F, /**< OTA service has not been initialised yet. */
} mtb_ble_ota_error_t;

/**
 * @brief Register the BLE OTA service on an existing NimBLE server.
 * @param server  Pointer to the NimBLEServer instance to attach the service to.
 * @return true   on success.
 * @return false  if characteristic or service creation fails.
 */
bool mtb_Ble_Ota_Init(NimBLEServer *server);

/**
 * @brief Unregister and free the BLE OTA service resources.
 */
void mtb_Ble_Ota_Deinit(void);

/**
 * @brief Query whether an OTA session is currently in progress.
 * @return true if a START command has been received and data transfer is underway.
 */
bool mtb_Ble_Ota_Is_Active(void);

/**
 * @brief Query whether the OTA service is initialised and able to accept a new session.
 * @return true if the service was successfully initialised via mtb_Ble_Ota_Init().
 */
bool mtb_Ble_Ota_Is_Ready(void);

/**
 * @brief Enable or disable the OTA service from accepting new sessions.
 * @param enable  true to allow new OTA sessions, false to block them.
 */
void mtb_Ble_Ota_Set_Enabled(bool enable);

/**
 * @brief Query whether the OTA service is currently enabled.
 * @return true if new OTA sessions are permitted.
 */
bool mtb_Ble_Ota_Is_Enabled(void);

/**
 * @brief Enable the OTA service and place it in ready mode to accept the next START command.
 */
void mtb_Ble_Ota_Enter_Ready_Mode(void);

/**
 * @brief Abort any in-progress OTA session and reset the OTA state machine.
 */
void mtb_Ble_Ota_Abort(void);

/**
 * @brief Return the firmware image size declared in the most recent START command.
 * @return Expected total byte count, or 0 if no session is active.
 */
uint32_t mtb_Ble_Ota_Get_Expected_Size(void);

/**
 * @brief Return the number of firmware bytes received so far in the active session.
 * @return Cumulative bytes written to the OTA partition.
 */
uint32_t mtb_Ble_Ota_Get_Received_Size(void);

/**
 * @brief Return the running CRC-32 computed over all data chunks received so far.
 * @return CRC-32 accumulator value.
 */
uint32_t mtb_Ble_Ota_Get_Running_Crc(void);

#endif // MTB_BLE_OTA_H

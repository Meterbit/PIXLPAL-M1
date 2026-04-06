#include "mtb_ble_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_crc.h"
#include "esp_system.h"
#include "NimBLEDevice.h"

static const char *TAG = "BLE_OTA";

// BLE objects
static NimBLEService *otaService = nullptr;
static NimBLECharacteristic *otaCtrlChar = nullptr;
static NimBLECharacteristic *otaDataChar = nullptr;

// OTA state
static bool otaActive = false;
static bool otaEnabled = false;

static esp_ota_handle_t otaHandle = 0;
static const esp_partition_t *otaPartition = nullptr;

static uint32_t expectedSize = 0;
static uint32_t receivedSize = 0;
static uint32_t runningCrc = 0;


// ===================== INTERNAL HELPERS =====================

static uint32_t readLE32(const uint8_t *buf)
{
    return ((uint32_t)buf[0]) |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

static void resetState()
{
    otaHandle = 0;
    otaPartition = nullptr;
    otaActive = false;
    expectedSize = 0;
    receivedSize = 0;
    runningCrc = 0;
}

static void sendStatus(uint8_t code)
{
    if (!otaCtrlChar) return;

    otaCtrlChar->setValue(&code, 1);
    otaCtrlChar->notify();
}

static void sendError(uint8_t err)
{
    if (!otaCtrlChar) return;

    uint8_t rsp[2] = {MTB_BLE_OTA_STATUS_ERROR, err};
    otaCtrlChar->setValue(rsp, 2);
    otaCtrlChar->notify();
}

static void abortOTA()
{
    if (otaActive) {
        esp_ota_abort(otaHandle);
    }
    resetState();
}


// ===================== CALLBACKS =====================

class OtaCtrlCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) override
    {
        if (!otaEnabled) {
            sendError(MTB_BLE_OTA_ERR_SERVICE_NOT_READY);
            return;
        }

        std::string val = pCharacteristic->getValue();

        if (val.size() < 1) {
            sendError(MTB_BLE_OTA_ERR_INVALID_CTRL_PACKET);
            return;
        }

        const uint8_t *buf = (const uint8_t*)val.data();
        uint8_t cmd = buf[0];

        // START
        if (cmd == MTB_BLE_OTA_CMD_START) {

            if (val.size() != 5) {
                sendError(MTB_BLE_OTA_ERR_BAD_START_LENGTH);
                return;
            }

            expectedSize = readLE32(&buf[1]);

            otaPartition = esp_ota_get_next_update_partition(NULL);
            if (!otaPartition) {
                sendError(MTB_BLE_OTA_ERR_NO_PARTITION);
                return;
            }

            if (esp_ota_begin(otaPartition, expectedSize, &otaHandle) != ESP_OK) {
                sendError(MTB_BLE_OTA_ERR_BEGIN_FAILED);
                return;
            }

            receivedSize = 0;
            runningCrc = 0;
            otaActive = true;

            ESP_LOGI(TAG, "OTA START size=%lu", expectedSize);
            sendStatus(MTB_BLE_OTA_STATUS_START_ACK);
            return;
        }

        // COMPLETE
        if (cmd == MTB_BLE_OTA_CMD_COMPLETE) {

            if (!otaActive) {
                sendError(MTB_BLE_OTA_ERR_NOT_ACTIVE);
                return;
            }

            if (val.size() != 5) {
                sendError(MTB_BLE_OTA_ERR_BAD_COMPLETE_LENGTH);
                abortOTA();
                return;
            }

            uint32_t expectedCrc = readLE32(&buf[1]);

            if (receivedSize != expectedSize) {
                sendError(MTB_BLE_OTA_ERR_SIZE_MISMATCH);
                abortOTA();
                return;
            }

            if (runningCrc != expectedCrc) {
                sendError(MTB_BLE_OTA_ERR_CRC_MISMATCH);
                abortOTA();
                return;
            }

            if (esp_ota_end(otaHandle) != ESP_OK) {
                sendError(MTB_BLE_OTA_ERR_END_FAILED);
                abortOTA();
                return;
            }

            if (esp_ota_set_boot_partition(otaPartition) != ESP_OK) {
                sendError(MTB_BLE_OTA_ERR_SET_BOOT_FAILED);
                abortOTA();
                return;
            }

            otaActive = false;
            sendStatus(MTB_BLE_OTA_STATUS_COMPLETE_ACK);

            ESP_LOGI(TAG, "OTA COMPLETE → rebooting");
            delay(500);
            esp_restart();
        }
        sendError(MTB_BLE_OTA_ERR_UNKNOWN_COMMAND);
    }
};

class OtaDataCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) override
    {
        if (!otaActive) {
            sendError(MTB_BLE_OTA_ERR_DATA_BEFORE_START);
            return;
        }

        std::string val = pCharacteristic->getValue();
        if (val.empty()) {
            sendError(MTB_BLE_OTA_ERR_EMPTY_CHUNK);
            return;
        }

        if (esp_ota_write(otaHandle, val.data(), val.size()) != ESP_OK) {
            sendError(MTB_BLE_OTA_ERR_WRITE_FAILED);
            abortOTA();
            return;
        }

        runningCrc = esp_crc32_le(runningCrc,
                                 (const uint8_t*)val.data(),
                                 val.size());

        receivedSize += val.size();
    }
};


// ===================== PUBLIC API =====================

bool mtb_Ble_Ota_Init(NimBLEServer *server)
{
    if (!server) return false;

    otaService = server->createService(MTB_BLE_OTA_SERVICE_UUID);
    if (otaService == nullptr) return false;

    otaCtrlChar = otaService->createCharacteristic(
        MTB_BLE_OTA_CTRL_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
    );

    otaDataChar = otaService->createCharacteristic(
        MTB_BLE_OTA_DATA_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );

    if (otaCtrlChar == nullptr || otaDataChar == nullptr) {
        return false;
    }

    otaCtrlChar->setCallbacks(new OtaCtrlCallbacks());
    otaDataChar->setCallbacks(new OtaDataCallbacks());

    otaService->start();

    resetState();
    otaEnabled = false;

    ESP_LOGI(TAG, "OTA service initialized");
    return true;
}

void mtb_Ble_Ota_Deinit(void)
{
    abortOTA();
    otaEnabled = false;
}

bool mtb_Ble_Ota_Is_Active(void)
{
    return otaActive;
}

bool mtb_Ble_Ota_Is_Ready(void)
{
    return otaEnabled && otaService != nullptr;
}

void mtb_Ble_Ota_Set_Enabled(bool enable)
{
    otaEnabled = enable;
}

bool mtb_Ble_Ota_Is_Enabled(void)
{
    return otaEnabled;
}

void mtb_Ble_Ota_Enter_Ready_Mode(void)
{
    otaEnabled = true;
}

void mtb_Ble_Ota_Abort(void)
{
    abortOTA();
}

uint32_t mtb_Ble_Ota_Get_Expected_Size(void)
{
    return expectedSize;
}

uint32_t mtb_Ble_Ota_Get_Received_Size(void)
{
    return receivedSize;
}

uint32_t mtb_Ble_Ota_Get_Running_Crc(void)
{
    return runningCrc;
}
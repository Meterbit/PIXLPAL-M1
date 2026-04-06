#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <Arduino.h>
#include "NimBLEDevice.h"
#include "driver/gpio.h"
#include <HardwareSerial.h>
#include <esp_wifi.h>
#include "mtb_nvs.h"
#include "mtb_text_scroll.h"
#include "mtb_engine.h"
#include "mtb_ble.h"


#define BLE_COMM_QUEUE_SIZE 3         // INCREASE THIS IF YOU ANTICIPATE A HIGHER VOLUME OF BLE MESSAGES TO BE TO BE SENT FROM APP TO PIXLPAL ESPECIALLY FOR APPCOM.

static const char TAG[] = "BLE_COMM";

bool isDisconnected = true;

EXT_RAM_BSS_ATTR JsonDocument dCommand;

EXT_RAM_BSS_ATTR TaskHandle_t ble_SetCom_Parser_Task_Handle = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t setCom_queue = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t appCom_queue = NULL;

uint16_t connHandle;

// BLE SECTION
EXT_RAM_BSS_ATTR NimBLEServer *pServer = NULL;
EXT_RAM_BSS_ATTR NimBLEService *pService = NULL;

EXT_RAM_BSS_ATTR NimBLECharacteristic *setCom_characteristic = NULL;
EXT_RAM_BSS_ATTR NimBLECharacteristic *appCom_characteristic = NULL;

EXT_RAM_BSS_ATTR mtb_BleCom_Data_Trans_t setCom_data;
EXT_RAM_BSS_ATTR mtb_BleCom_Data_Trans_t appCom_data;

EXT_RAM_BSS_ATTR String appValue;
EXT_RAM_BSS_ATTR String setValue;


#define PXP_BLE_SERVICE_UUID "73f3ee85-31f7-4f5a-bd9c-3b469dff65c7"
#define SETCOM_CHARACTERISTIC_UUID "472a6244-3bb8-4a7e-a107-4b47dea92bc3"
#define APPCOM_CHARACTERISTIC_UUID "c8f1eead-48b0-449d-accb-5fdb87c4b566"

//**** OTA BLE IMPLEMENTATIONS ************************************************************************************************ */

#include "esp_ota_ops.h"
#include "esp_crc.h"
#include "esp_partition.h"
#include "esp_system.h"

#define OTA_SERVICE_UUID   "487d0950-41b9-4c57-ad09-a46ac47e2150"
#define OTA_CTRL_UUID      "487d0950-41b9-4c57-ad09-a46ac47e2151"
#define OTA_DATA_UUID      "487d0950-41b9-4c57-ad09-a46ac47e2152"

EXT_RAM_BSS_ATTR NimBLEService *otaService = nullptr;
EXT_RAM_BSS_ATTR NimBLECharacteristic *otaCtrlChar = nullptr;
EXT_RAM_BSS_ATTR NimBLECharacteristic *otaDataChar = nullptr;

static esp_ota_handle_t g_otaHandle = 0;
static const esp_partition_t *g_otaPartition = nullptr;
static bool g_otaActive = false;
static uint32_t g_expectedSize = 0;
static uint32_t g_receivedSize = 0;
static uint32_t g_runningCrc = 0;


static void otaSendError(uint8_t err)
{
    if (otaCtrlChar == nullptr) return;

    uint8_t rsp[2] = {0xFF, err};  // matches library style: ERROR + code
    otaCtrlChar->setValue(rsp, sizeof(rsp));
    otaCtrlChar->notify();
}

static void otaSendStatus(uint8_t code)
{
    if (otaCtrlChar == nullptr) return;

    uint8_t rsp[1] = {code};
    otaCtrlChar->setValue(rsp, sizeof(rsp));
    otaCtrlChar->notify();
}

static void otaAbortSession()
{
    if (g_otaActive) {
        esp_ota_abort(g_otaHandle);
    }

    g_otaHandle = 0;
    g_otaPartition = nullptr;
    g_otaActive = false;
    g_expectedSize = 0;
    g_receivedSize = 0;
    g_runningCrc = 0;
}


class OtaControlCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) override
    {
        std::string value = pCharacteristic->getValue();

        if (value.size() < 1) {
            otaSendError(0x01); // invalid control packet
            return;
        }

        const uint8_t *buf = reinterpret_cast<const uint8_t *>(value.data());
        uint8_t cmd = buf[0];

        // START = [0x01][size0][size1][size2][size3]
        if (cmd == 0x01) {
            if (value.size() != 5) {
                otaSendError(0x02); // bad START length
                return;
            }

            if (g_otaActive) {
                otaAbortSession();
            }

            g_expectedSize = (uint32_t)buf[1]
                           | ((uint32_t)buf[2] << 8)
                           | ((uint32_t)buf[3] << 16)
                           | ((uint32_t)buf[4] << 24);

            g_otaPartition = esp_ota_get_next_update_partition(nullptr);
            if (g_otaPartition == nullptr) {
                otaSendError(0x03); // no partition
                return;
            }

            esp_err_t err = esp_ota_begin(g_otaPartition, g_expectedSize, &g_otaHandle);
            if (err != ESP_OK) {
                otaSendError(0x04); // begin failed
                return;
            }

            g_receivedSize = 0;
            g_runningCrc = 0;
            g_otaActive = true;

            ESP_LOGI(TAG, "BLE OTA started. Expected size: %lu", (unsigned long)g_expectedSize);
            otaSendStatus(0x01);
            return;
        }

        // COMPLETE = [0x02][crc0][crc1][crc2][crc3]
        if (cmd == 0x02) {
            if (!g_otaActive) {
                otaSendError(0x05); // no active OTA
                return;
            }

            if (value.size() != 5) {
                otaSendError(0x06); // bad COMPLETE length
                otaAbortSession();
                return;
            }

            uint32_t expectedCrc = (uint32_t)buf[1]
                                 | ((uint32_t)buf[2] << 8)
                                 | ((uint32_t)buf[3] << 16)
                                 | ((uint32_t)buf[4] << 24);

            if (g_receivedSize != g_expectedSize) {
                otaSendError(0x07); // size mismatch
                otaAbortSession();
                return;
            }

            if (g_runningCrc != expectedCrc) {
                otaSendError(0x08); // CRC mismatch
                otaAbortSession();
                return;
            }

            esp_err_t err = esp_ota_end(g_otaHandle);
            if (err != ESP_OK) {
                otaSendError(0x09); // end failed
                otaAbortSession();
                return;
            }

            err = esp_ota_set_boot_partition(g_otaPartition);
            if (err != ESP_OK) {
                otaSendError(0x0A); // boot partition set failed
                otaAbortSession();
                return;
            }

            g_otaActive = false;
            otaSendStatus(0x02); // success
            ESP_LOGI(TAG, "BLE OTA complete. Rebooting...");

            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
            return;
        }

        otaSendError(0x0B); // unknown command
    }
};

class OtaDataCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) override
    {
        if (!g_otaActive) {
            otaSendError(0x0C); // data received before START
            return;
        }

        std::string value = pCharacteristic->getValue();
        if (value.empty()) {
            otaSendError(0x0D); // empty chunk
            return;
        }

        esp_err_t err = esp_ota_write(g_otaHandle, value.data(), value.size());
        if (err != ESP_OK) {
            otaSendError(0x0E); // write failed
            otaAbortSession();
            return;
        }

        g_runningCrc = esp_crc32_le(g_runningCrc,
                                    reinterpret_cast<const uint8_t *>(value.data()),
                                    value.size());

        g_receivedSize += value.size();

        ESP_LOGI(TAG, "BLE OTA chunk: %u / %u",
                 (unsigned)g_receivedSize,
                 (unsigned)g_expectedSize);
    }
};

//**************************************************************************************************** */

EXT_RAM_BSS_ATTR Mtb_Services *mtb_Sett_BleComm_Parser_Sv = new Mtb_Services(ble_SetCom_Parse_Task, &ble_SetCom_Parser_Task_Handle, "bleSetCom_parser_task", 4096, 4); 
EXT_RAM_BSS_ATTR Mtb_Services *mtb_App_BleComm_Parser_Sv = new Mtb_Services(ble_AppCom_Parse_Task, &ble_AppCom_Parser_Task_Handle, "bleAppCom_Parser_task", 4096, 4);

class MyServerCallbacks : public NimBLEServerCallbacks{
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo){
    isDisconnected = false;
    Mtb_Applications::bleCentralContd = true;
    connHandle = connInfo.getConnHandle();
    mtb_Show_Status_Bar_Icon({"/batIcons/phoneCont.png", 18, 1});
    mtb_Read_Nvs_Struct("pxpBleDevName", pxp_BLE_Name, sizeof(pxp_BLE_Name));
    mtb_Current_Ble_Device(pxp_BLE_Name);
    ESP_LOGI(TAG, "Connected\n");
  };

  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo, int reason){
    ESP_LOGI(TAG, "Disconnection detected.\n");
    isDisconnected = true;
    Mtb_Applications::bleCentralContd = false;
    mtb_Show_Status_Bar_Icon({"/batIcons/btOn.png", 18, 1});
    // Start advertising
    NimBLEDevice::startAdvertising(); // Start advertising
  };

  void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override {
    Mtb_Applications::bleCentralNegotiatedMtu = MTU;
    ESP_LOGI(TAG, "Negotiated MTU: %u\n", MTU);
  }
};

class CharacteristicsCallbacks : public NimBLECharacteristicCallbacks{
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo){

    if (g_otaActive) {
        ESP_LOGW(TAG, "Ignoring normal BLE command during OTA");
        return;
    }

  ESP_LOGI(TAG, "BLEData Received : %s \n", pCharacteristic->getValue().c_str());

if(pCharacteristic == setCom_characteristic){
      setValue = pCharacteristic->getValue().c_str();
      //ESP_LOGI(TAG,  "SetCom Received Message : %s \n", setValue.c_str());      

      setCom_data.pay_size = pCharacteristic->getLength();
      setCom_data.payload = heap_caps_calloc(pCharacteristic->getLength() + 1, sizeof(uint8_t), MALLOC_CAP_SPIRAM);
      memcpy(setCom_data.payload, pCharacteristic->getValue(), pCharacteristic->getLength());
      xQueueSend(setCom_queue, &setCom_data, portMAX_DELAY);
      mtb_Launch_This_Service(mtb_Sett_BleComm_Parser_Sv);

    } else if (pCharacteristic == appCom_characteristic){
      appValue = pCharacteristic->getValue().c_str();
      //ESP_LOGI(TAG,  "AppCom Received Message : %s \n", appValue.c_str());

      appCom_data.pay_size = pCharacteristic->getLength();
      appCom_data.payload = heap_caps_calloc(pCharacteristic->getLength() + 1, sizeof(uint8_t), MALLOC_CAP_SPIRAM);
      memcpy(appCom_data.payload, pCharacteristic->getValue(), pCharacteristic->getLength());
      xQueueSend(appCom_queue, &appCom_data, portMAX_DELAY);
      mtb_Launch_This_Service(mtb_App_BleComm_Parser_Sv);
    } else ESP_LOGI(TAG, "PIXLPAL IS RECEIVING THE COMMAND, BUT IT'S NOT BEING RIGHTLY PARSED.\n");
  }
};

void mtb_Ble_Comm_Init(void){
    // ---------------- OTA SERVICE ----------------
    otaService = pServer->createService(OTA_SERVICE_UUID);

    otaCtrlChar = otaService->createCharacteristic(
        OTA_CTRL_UUID,
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY
    );

    otaDataChar = otaService->createCharacteristic(
        OTA_DATA_UUID,
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::WRITE_NR
    );

    otaCtrlChar->setCallbacks(new OtaControlCallbacks());
    otaDataChar->setCallbacks(new OtaDataCallbacks());

    otaService->start();

 // ********************************************************************************************************

  appValue = "0";
  setValue = "1";

  if(setCom_queue == NULL) setCom_queue = xQueueCreate(BLE_COMM_QUEUE_SIZE, sizeof(mtb_BleCom_Data_Trans_t));
  if(appCom_queue == NULL) appCom_queue = xQueueCreate(BLE_COMM_QUEUE_SIZE, sizeof(mtb_BleCom_Data_Trans_t));

    // Create the BLE Device
    mtb_Read_Nvs_Struct("pxpBleDevName", pxp_BLE_Name, sizeof(pxp_BLE_Name));
    NimBLEDevice::init(pxp_BLE_Name);

    // Request MTU
    NimBLEDevice::setMTU(512); // Request MTU size of 512

    // Create the BLE Server
    pServer = NimBLEDevice::createServer();

    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
    pService = pServer->createService(PXP_BLE_SERVICE_UUID);
    delay(100);

    // Create a BLE Characteristic
    setCom_characteristic = pService->createCharacteristic(
        SETCOM_CHARACTERISTIC_UUID,
            NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::NOTIFY |
            NIMBLE_PROPERTY::INDICATE);

    appCom_characteristic = pService->createCharacteristic(
        APPCOM_CHARACTERISTIC_UUID,
            NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::NOTIFY |
            NIMBLE_PROPERTY::INDICATE);

    // Start the BLE service
    pService->start();

    // Start advertising
    // pServer->getAdvertising()->start();

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(PXP_BLE_SERVICE_UUID); // Add the service UUID to advertising
    pAdvertising->addServiceUUID(OTA_SERVICE_UUID);
    pAdvertising->enableScanResponse(true);       // Include scan response if needed

    mtb_Read_Nvs_Struct("pxpBleDevName", pxp_BLE_Name, sizeof(pxp_BLE_Name));
    pAdvertising->setName(pxp_BLE_Name); // Set the device name
    NimBLEDevice::startAdvertising(); // Start advertising


    setCom_characteristic->setValue("SetCom Xter Ready.");
    setCom_characteristic->setCallbacks(new CharacteristicsCallbacks());

    appCom_characteristic->setValue("AppCom Xter Ready.");
    appCom_characteristic->setCallbacks(new CharacteristicsCallbacks());

    Mtb_Applications::bleAdvertisingStatus = true;
    //mtb_Show_Status_Bar_Icon({"/batIcons/btOn.png", 18, 1});
}

void waitForDisconnections() {
    while (!isDisconnected) {
        delay(10);  // Wait until all clients are disconnected
    }
}

void mtb_Ble_Comm_Deinit() {
    // Disconnect clients and deinitialize BLE only if OTA is not active to avoid interrupting the OTA process.
    if (g_otaActive) {
        ESP_LOGW(TAG, "BLE deinit blocked: OTA in progress");
        return;
    }

    // Disconnect clients
    Serial.println("Waiting for disconnections...");
    if (pServer) {
          if(pServer->getConnectedCount() > 0){
            pServer->disconnect(pServer->getPeerInfoByHandle(connHandle));
            waitForDisconnections(); // Wait for disconnection callback
        }
    }

    // Stop advertising
    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    if (advertising) {
        advertising->stop();
    }
    delay(100);
    // Deinitialize BLE Device
    NimBLEDevice::deinit();

    Mtb_Applications::bleAdvertisingStatus = false;
    mtb_Show_Status_Bar_Icon({"/batIcons/wipe7x7.png", 18, 1}); 
}

void mtb_Ble_Change_Pxp_Ble_Name(const char* pxp_BLE_Name){
      // 1. Stop advertising
      NimBLEDevice::getAdvertising()->stop();

        delay(100);

      // 2. Set new device name
      NimBLEDevice::setDeviceName(pxp_BLE_Name);

      // 3. Update advertising data
      NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();

      NimBLEAdvertisementData advData;
      advData.setName(pxp_BLE_Name);   // VERY IMPORTANT

      pAdvertising->setAdvertisementData(advData);

      // 4. Restart advertising
      pAdvertising->start();
}

int bleSettingsComSend(const char* dRoute, const String& dMessage) {
      if(Mtb_Applications::bleCentralContd == false) return 0;
      setCom_characteristic->setValue(String(dRoute) + "|" + dMessage);
      ESP_LOGI(TAG, "BLE Settings Data Sent : %s \n", (String(dRoute) + "|" + dMessage).c_str());
      if (!setCom_characteristic->notify()) ESP_LOGW("BLE", "Notify failed for setCom_characteristic");
      return 1;
}

int bleApplicationComSend(const char* dRoute, const String& dMessage) {
    if (Mtb_Applications::bleCentralContd == false) return 0;
    if (appCom_characteristic == nullptr) return 0;

    String route = String(dRoute);

    uint16_t mtu = Mtb_Applications::bleCentralNegotiatedMtu;
    if (mtu < 23) mtu = 23;

    int maxPayload = mtu - 3;

    static uint32_t msgCounter = 0;
    uint32_t msgId = ++msgCounter;

    int totalLen = dMessage.length();

    String testHeader = route + "|" + String(msgId) + "|0|0|" + String(totalLen) + "|";
    int chunkDataSize = maxPayload - testHeader.length();

    if (chunkDataSize < 20) chunkDataSize = 20;

    int totalChunks = (totalLen + chunkDataSize - 1) / chunkDataSize;
    if (totalChunks == 0) totalChunks = 1;

    for (int chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        int start = chunkIndex * chunkDataSize;
        String chunkData = dMessage.substring(start, start + chunkDataSize);

        String packet =
            route + "|" +
            String(msgId) + "|" +
            String(chunkIndex) + "|" +
            String(totalChunks) + "|" +
            String(totalLen) + "|" +
            chunkData;

        appCom_characteristic->setValue(packet.c_str());

        if (!appCom_characteristic->notify()) {
            ESP_LOGW("BLE", "Notify failed for appCom_characteristic");
            return 0;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return 1;
}

int getIntegerAtIndex(const String& data, int index) {
    int currentIndex = 0;   // Track the current index of numbers
    int start = 0;          // Start of each number segment
    int end = 0;            // End of each number segment
    
    // Loop through the string to find the start and end of each segment
    while ((end = data.indexOf('/', start)) != -1) {
        if (currentIndex == index) {
            // Found the segment at the specified index
            return data.substring(start, end).toInt();
        }
        start = end + 1;   // Move start to the next character after '/'
        currentIndex++;
    }
    
    // Check the last segment (in case there's no trailing '/')
    if (currentIndex == index) {
        return data.substring(start).toInt();
    }
    
    // Return -1 if index is out of range
    return -1;
}

// This service self-terminates after one queue item.
// If continuous listening is needed, consider keeping it alive or using a persistent loop task.
void ble_SetCom_Parse_Task(void* dService){
  Mtb_Services *thisService = (Mtb_Services *)dService;
  mtb_BleCom_Data_Trans_t qMessage;
  DeserializationError dError;
  String specify_Settings;
  uint16_t dSetCategory = 0;

  while (xQueueReceive(setCom_queue, &qMessage, pdMS_TO_TICKS(500))){
    //ESP_LOGI(TAG, "Settings Payload is: %s\n", (char*) qMessage.payload);
    
    String dInstruction = String((char *)qMessage.payload);
    int charIndex = dInstruction.indexOf('|');             // find index of target character
    String specify_Settings = dInstruction.substring(0, charIndex);  // copy up to the target character
    String dJsonPayload = dInstruction.substring(++charIndex);

    dError = deserializeJson(dCommand, dJsonPayload);
    if(dError.code() == dError.Ok) dSetCategory = specify_Settings.toInt();
    else dSetCategory = 0xFFFF;

    switch (dSetCategory){
    case 1: systemSettings(dCommand);
      break;
    case 2: wifiSettings(dCommand);
      break;
    case 3: bleSettings(dCommand);
      break;
    case 4: softwareUpdate(dCommand);
      break;
    default: statusBarNotif.mtb_Scroll_This_Text("COMMAND IS NOT RECOGNISED.", YELLOW);
      break;
    }

    vTaskDelay(1); // Or yield inside the while loop
    free(qMessage.payload); // Free the allocated memory
    //qMessage.payload = NULL; // Set pointer to NULL to avoid dangling pointer
  }
  
  mtb_Delete_This_Service(thisService);
}

void ble_AppCom_Parse_Task(void* dService){
    Mtb_Services *thisService = (Mtb_Services *)dService;
    mtb_BleCom_Data_Trans_t qMessage;
    DeserializationError dError;
    String dNewAppParams;
    uint16_t dAppGen = 0;
    uint16_t dAppSpe = 0;
    uint16_t dCmd_num = 0;

    while(xQueueReceive(appCom_queue, &qMessage, pdMS_TO_TICKS(500))){
        //ESP_LOGI(TAG, "Application Payload is:  %s\n", (char*) qMessage.payload);
        String dInstruction = String((char *)qMessage.payload);
        int charIndex = dInstruction.indexOf('|');             // find index of target character
        String specific_Application = dInstruction.substring(0, charIndex);  // copy up to the target character
        String dJsonPayload = dInstruction.substring(++charIndex);

        Mtb_UserApp_t showAppUI{
            .GenApp = 0,
            .SpeApp = 1
            };

        ESP_LOGI(TAG, "The specific App is: %s\n", specific_Application.c_str());
        // ESP_LOGI(TAG, "The dPayload for App is: %s\n", dPayload.c_str());
 
        dAppGen = getIntegerAtIndex(specific_Application, 0);
        dAppSpe = getIntegerAtIndex(specific_Application, 1);

        //ESP_LOGI(TAG, "The dAppGen is: %d\n", dAppGen);
        //ESP_LOGI(TAG, "The dAppSpe is: %d\n", dAppSpe);

        if (dAppGen == activateUserApp.GenApp && dAppSpe == activateUserApp.SpeApp){

            dError = deserializeJson(dCommand, dJsonPayload);
            if(dError.code() == dError.Ok) dCmd_num = dCommand["app_command"];
            else dCmd_num = 0xFFFF;

            switch (dCmd_num){
            case 0: if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[0] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[0](dCommand);
                mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
                break;
            case 1: if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[1] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[1](dCommand);
                mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
                break;
            case 2: if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[2] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[2](dCommand);
                mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
                break;
            case 3: if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[3] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[3](dCommand);
                mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
                break;
            case 4: if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[4] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[4](dCommand);
                mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
              break;
            case 5: if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[5] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[5](dCommand);
                mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
                break;
            case 6: if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[6] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[6](dCommand);
                mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
                break;
            case 7: if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[7] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[7](dCommand);
                mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
                break;
            case 8: if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[8] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[8](dCommand);
                mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
                break;
            case 9: if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[9] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[9](dCommand);
                mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
                break;
            case 10: if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[10] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[10](dCommand);
                mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
                break;
            case 11: if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[11] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[11](dCommand);
                mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
                break;
            // case 12: if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[12] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[12](dCommand);     // These are for extras or future upgrades.
            //     mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
            //     break;
            // case 13:  if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[13] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[13](dCommand);
            //     mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
            //     break;
            // case 14:  if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[14] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[14](dCommand);
            //     mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
            //     break;
            // case 15:  if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[15] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[15](dCommand);
            //     mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
            //     break;
            // case 16:  if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[16] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[16](dCommand);
            //     mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
            //     break;
            // case 17:  if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[17] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[17](dCommand);
            //     mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
            //     break;
            // case 18:  if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[18] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[18](dCommand);
            //     mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
            //     break;
            // case 19:  if(Mtb_Applications::currentRunningApp->bleAppComServiceFns[19] != nullptr) Mtb_Applications::currentRunningApp->bleAppComServiceFns[19](dCommand);
            //     mtb_Ble_App_Cmd_Respond_Success(specific_Application.c_str(), dCmd_num);
            //     break;

            case 250:
                bleApplicationComSend(specific_Application.c_str(), "{\"app_command\": 250}");
                break;
            case 251:
                bleApplicationComSend(specific_Application.c_str(), "{\"app_command\": 251}");
                break;
            case 252:
                Mtb_Applications::showAppUI_OR_LaunchApp = SHOW_APP_UI;
                showAppUI.GenApp = dAppGen;
                showAppUI.SpeApp = dAppSpe;
                mtb_General_App_Register(showAppUI);
                break;
            case 255:
                statusBarNotif.mtb_Scroll_This_Text("APP IS ALREADY ACTIVE", CYAN);
                bleApplicationComSend(specific_Application.c_str(), "{\"app_command\": 255}");
                break;
            default: statusBarNotif.mtb_Scroll_This_Text("ERROR: ASSESS COMMAND PARAMETERS", YELLOW);
                break;
            }
        }else{
          dError = deserializeJson(dCommand, dJsonPayload);
          dCmd_num = dCommand["app_command"];

          if(dError.code() == dError.Ok && dCmd_num == 250){
              bleApplicationComSend(specific_Application.c_str(), "{\"app_command\": 250}");
          }else if(dError.code() == dError.Ok && dCmd_num == 251){
              bleApplicationComSend(specific_Application.c_str(), "{\"app_command\": 251}");
          }else if(dError.code() == dError.Ok && dCmd_num == 252){
              showAppUI.GenApp = dAppGen;
              showAppUI.SpeApp = dAppSpe;
              Mtb_Applications::showAppUI_OR_LaunchApp = SHOW_APP_UI; 
              mtb_General_App_Register(showAppUI);
          }else if (dError.code() == dError.Ok && dCmd_num == 255){
              activateUserApp.GenApp = dAppGen;
              activateUserApp.SpeApp = dAppSpe;
              mtb_Write_Nvs_Struct("activateUserApp", &activateUserApp, sizeof(Mtb_UserApp_t));
              mtb_General_App_Register(activateUserApp);
              bleApplicationComSend(specific_Application.c_str(), "{\"app_command\": 253}");
          }else{
              bleApplicationComSend(specific_Application.c_str(), "{\"app_command\": 254}");
              statusBarNotif.mtb_Scroll_This_Text("TAP 'LAUNCH' TO START APP", MAGENTA);
          } 
        }
    vTaskDelay(1);
    free(qMessage.payload);
    //qMessage.payload = NULL; // Set pointer to NULL to avoid dangling pointer    
  }
  mtb_Delete_This_Service(thisService);
}
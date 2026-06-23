/**
 * @file mtb_ble.cpp
 * @brief NimBLE GATT server implementation for the PIXLPAL-M1 BLE communication subsystem.
 *
 * Creates a BLE server with two characteristics (settings channel and application channel),
 * manages the connection lifecycle via MyServerCallbacks, and implements the parser tasks
 * (ble_SetCom_Parse_Task / ble_AppCom_Parse_Task) that deserialise incoming JSON payloads
 * and dispatch them to the appropriate settings or application handler.
 */
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
#include "mtb_ble_ota.h"


#define BLE_COMM_QUEUE_SIZE 3         // INCREASE THIS IF YOU ANTICIPATE A HIGHER VOLUME OF BLE MESSAGES TO BE TO BE SENT FROM APP TO PIXLPAL ESPECIALLY FOR APPCOM.

static const char TAG[] = "BLE_COMM";

bool isDisconnected = true;

EXT_RAM_BSS_ATTR JsonDocument dCommand;

EXT_RAM_BSS_ATTR TaskHandle_t ble_SetCom_Parser_Task_Handle = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t ble_AppCom_Parser_Task_Handle = NULL;
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
    // ESP_LOGI(TAG, "Connected\n");
  };

  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo, int reason){
    // ESP_LOGI(TAG, "Disconnection detected.\n");
    isDisconnected = true;
    Mtb_Applications::bleCentralContd = false;
    mtb_Show_Status_Bar_Icon({"/batIcons/btOn.png", 18, 1});
//
    // If an OTA transfer was in progress, abort it.
    if (mtb_Ble_Ota_Is_Active()) {
      ESP_LOGW(TAG, "Client disconnected during OTA. Aborting OTA session.");
      mtb_Ble_Ota_Abort();
    }

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

  // Block normal SETCOM / APPCOM traffic while OTA is running.
  if (mtb_Ble_Ota_Is_Active()) {
      ESP_LOGW(TAG, "Ignoring normal BLE write because OTA is active");
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

    // Initialize BLE OTA service on the same server
    mtb_Ble_Ota_Init(pServer);

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

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(PXP_BLE_SERVICE_UUID); // Add main service UUID to advertising
    pAdvertising->addServiceUUID(MTB_BLE_OTA_SERVICE_UUID);
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
    // Prevent BLE shutdown while OTA is in progress
    if (mtb_Ble_Ota_Is_Active()) {
        ESP_LOGW(TAG, "BLE deinit blocked because OTA is in progress");
        return;
    }

    // Disconnect clients
    Serial.println("Waiting for disconnections...");
    if (pServer) {
          if(pServer->getConnectedCount() > 0){
            pServer->disconnect(connHandle);
            waitForDisconnections(); // Wait for disconnection callback
        }
    }

    // Stop advertising
    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    if (advertising) {
        advertising->stop();
    }
    delay(100);

    // Deinitialize OTA state before BLE deinit
    mtb_Ble_Ota_Deinit();

    // Deinitialize BLE Device
    NimBLEDevice::deinit();

    Mtb_Applications::bleAdvertisingStatus = false;
    mtb_Show_Status_Bar_Icon({"/batIcons/wipe7x7.png", 18, 1}); 
}

void mtb_Ble_Change_Pxp_Ble_Name(const char* pxp_BLE_Name){
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    if (pAdvertising == nullptr) return;

    pAdvertising->stop();
    delay(100);

    NimBLEDevice::setDeviceName(pxp_BLE_Name);

    NimBLEAdvertisementData advData;
    advData.setName(pxp_BLE_Name);
    advData.addServiceUUID(PXP_BLE_SERVICE_UUID);
    advData.addServiceUUID(MTB_BLE_OTA_SERVICE_UUID);

    pAdvertising->setAdvertisementData(advData);
    pAdvertising->enableScanResponse(true);
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
        ESP_LOGI(TAG, "BLE Application Data Sent (Chunk %d/%d): %s\n", chunkIndex + 1, totalChunks, packet.c_str());
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
  Mtb_Services *THIS_SERVICE = (Mtb_Services *)dService;
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
    case 0: app_Cycle_Settings(dCommand);
    break;
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
  
  mtb_Delete_This_Service(THIS_SERVICE);
}

void ble_AppCom_Parse_Task(void* dService){
    Mtb_Services *THIS_SERVICE = (Mtb_Services *)dService;
    mtb_BleCom_Data_Trans_t qMessage;
    DeserializationError dError;
    String dNewAppParams;
    uint16_t dAppGen = 0;
    uint16_t dAppSpe = 0;
    Mtb_User_AppID_t appHolder;
    uint16_t dCmd_num = 0;

    while(xQueueReceive(appCom_queue, &qMessage, pdMS_TO_TICKS(500))){
        //ESP_LOGI(TAG, "Application Payload is:  %s\n", (char*) qMessage.payload);
        String dInstruction = String((char *)qMessage.payload);
        int charIndex = dInstruction.indexOf('|');             // find index of target character
        String specific_Application = dInstruction.substring(0, charIndex);  // copy up to the target character
        String dJsonPayload = dInstruction.substring(++charIndex);

        ESP_LOGI(TAG, "The specific App is: %s\n", specific_Application.c_str());
        // ESP_LOGI(TAG, "The dPayload for App is: %s\n", dPayload.c_str());
 
        appHolder.GenApp = getIntegerAtIndex(specific_Application, 0);
        appHolder.SpeApp = getIntegerAtIndex(specific_Application, 1);

        //ESP_LOGI(TAG, "The dAppGen is: %d\n", dAppGen);
        //ESP_LOGI(TAG, "The dAppSpe is: %d\n", dAppSpe);

        if (appHolder.GenApp == Mtb_Applications::currentRunningApp->appID.GenApp && appHolder.SpeApp == Mtb_Applications::currentRunningApp->appID.SpeApp){

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

            case 252:
                mtb_Identify_App_By_ID(appHolder, SHOW_PXP_APP_UI);
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

          if(dError.code() == dError.Ok && dCmd_num == 252){
            mtb_Identify_App_By_ID(appHolder, SHOW_PXP_APP_UI);
          }else if (dError.code() == dError.Ok && dCmd_num == 255){
            String jsonString;
            JsonDocument doc;
            doc["app_command"] = 253;
            doc["appCyclingActive"] = cycling_Apps.appsCycleActive;
            if(!cycling_Apps.appsCycleActive) mtb_Identify_App_By_ID(appHolder, LAUNCH_PXP_APP);
            bleApplicationComSend(specific_Application.c_str(), doc.as<String>());
          }else{
              bleApplicationComSend(specific_Application.c_str(), "{\"app_command\": 254}");
              statusBarNotif.mtb_Scroll_This_Text("TAP 'LAUNCH' TO START APP", MAGENTA);
          } 
        }
    vTaskDelay(1);
    free(qMessage.payload);
    //qMessage.payload = NULL; // Set pointer to NULL to avoid dangling pointer    
  }
  mtb_Delete_This_Service(THIS_SERVICE);
}
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

bool isDisconnected = true;

DynamicJsonDocument dCommand(1024);

EXT_RAM_BSS_ATTR TaskHandle_t ble_SetCom_Parser_Task_Handle = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t setCom_queue = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t appCom_queue = NULL;

uint16_t connHandle;

// BLE SECTION
NimBLEServer *pServer = NULL;
NimBLEService *pService = NULL;

NimBLECharacteristic *setCom_characteristic = NULL;
NimBLECharacteristic *appCom_characteristic = NULL;

bleCom_Data_Trans_t setCom_data;
bleCom_Data_Trans_t appCom_data;

int bleCom_queue_size = 6;
String appValue = "0";
String setValue = "1";


// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define PXP_BLE_SERVICE_UUID "73f3ee85-31f7-4f5a-bd9c-3b469dff65c7"
#define SETCOM_CHARACTERISTIC_UUID "472a6244-3bb8-4a7e-a107-4b47dea92bc3"
#define APPCOM_CHARACTERISTIC_UUID "c8f1eead-48b0-449d-accb-5fdb87c4b566"

EXT_RAM_BSS_ATTR Services *ble_SetCom_Parse_Sv = new Services(ble_SetCom_Parse_Task, &ble_SetCom_Parser_Task_Handle, "bleSetCom_parser_task", 6144, 4); // THIS FUNCTIONS CANNOT BE AN PSRAM MEMORY BECAUSE THEY MIGHT ATTEMPT TO WRITE THE ONBOARD FLASH
EXT_RAM_BSS_ATTR Service_With_Fns *ble_AppCom_Parser_Sv = new Service_With_Fns(ble_AppCom_Parse_Task, &ble_AppCom_Parser_Task_Handle, "bleAppCom_Parser_task", 6144, 4); // THIS FUNCTIONS CANNOT BE AN PSRAM MEMORY BECAUSE THEY MIGHT ATTEMPT TO WRITE THE ONBOARD FLASH

class MyServerCallbacks : public NimBLEServerCallbacks{
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo){
    // Request MTU
    NimBLEDevice::setMTU(512); // Request MTU size of 512
    isDisconnected = false;
    Applications::bleCentralContd = true;
    connHandle = connInfo.getConnHandle();
    showStatusBarIcon({"/batIcons/phoneCont.png", 18, 1});
    read_struct_from_nvs("pxpBleDevName", pxp_BLE_Name, sizeof(pxp_BLE_Name));
    current_BleDevice(pxp_BLE_Name);
    //printf("Connected\n");
  };

  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo, int reason){
    //printf("Disconnection detected.\n");
    isDisconnected = true;
    Applications::bleCentralContd = false;
    showStatusBarIcon({"/batIcons/btOn.png", 18, 1});
    // Start advertising
    NimBLEDevice::startAdvertising(); // Start advertising
  }
};

class CharacteristicsCallbacks : public NimBLECharacteristicCallbacks{
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo){

    printf("Value Written: %s \n", pCharacteristic->getValue().c_str());

if(pCharacteristic == setCom_characteristic){
      setValue = pCharacteristic->getValue().c_str();
      //printf( "SetCom Received Message : %s \n", setValue.c_str());      

      setCom_data.pay_size = pCharacteristic->getLength();
      setCom_data.payload = heap_caps_calloc(pCharacteristic->getLength() + 1, sizeof(uint8_t), MALLOC_CAP_SPIRAM);
      memcpy(setCom_data.payload, pCharacteristic->getValue(), pCharacteristic->getLength());
      xQueueSend(setCom_queue, &setCom_data, portMAX_DELAY);
      start_This_Service(ble_SetCom_Parse_Sv);

    } else if (pCharacteristic == appCom_characteristic){
      appValue = pCharacteristic->getValue().c_str();
      //printf( "AppCom Received Message : %s \n", appValue.c_str());

      appCom_data.pay_size = pCharacteristic->getLength();
      appCom_data.payload = heap_caps_calloc(pCharacteristic->getLength() + 1, sizeof(uint8_t), MALLOC_CAP_SPIRAM);
      memcpy(appCom_data.payload, pCharacteristic->getValue(), pCharacteristic->getLength());
      xQueueSend(appCom_queue, &appCom_data, portMAX_DELAY);
      start_This_Service(ble_AppCom_Parser_Sv);
    } else printf("PIXLPAL IS RECEIVING THE COMMAND, BUT IT'S NOT BEING RIGHTLY PARSED.\n");
  }
};

void initBLE_Communication(void){
  if(setCom_queue == NULL) setCom_queue = xQueueCreate(bleCom_queue_size,sizeof(bleCom_Data_Trans_t));     // A queue of character pointers
  if(appCom_queue == NULL) appCom_queue = xQueueCreate(bleCom_queue_size,sizeof(bleCom_Data_Trans_t));     // A queue of character pointers

    // Create the BLE Device
    read_struct_from_nvs("pxpBleDevName", pxp_BLE_Name, sizeof(pxp_BLE_Name));
    NimBLEDevice::init(pxp_BLE_Name);
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
    pAdvertising->enableScanResponse(true);       // Include scan response if needed

    read_struct_from_nvs("pxpBleDevName", pxp_BLE_Name, sizeof(pxp_BLE_Name));
    pAdvertising->setName(pxp_BLE_Name); // Set the device name
    NimBLEDevice::startAdvertising(); // Start advertising

    setCom_characteristic->setValue("Setting Xter Ready.");
    setCom_characteristic->setCallbacks(new CharacteristicsCallbacks());

    appCom_characteristic->setValue("AppCom Xter Ready.");
    appCom_characteristic->setCallbacks(new CharacteristicsCallbacks());

    Applications::bleAdvertisingStatus = true;
    //showStatusBarIcon({"/batIcons/btOn.png", 18, 1});
}

void waitForDisconnections() {
    while (!isDisconnected) {
        delay(10);  // Wait until all clients are disconnected
    }
}

void deinitBLE_Communication() {
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

    Applications::bleAdvertisingStatus = false;
    showStatusBarIcon({"/batIcons/wipe7x7.png", 18, 1}); 
}

int bleSettingsComSend(const char* dRoute, String dMessage){
      if(Applications::bleCentralContd == false) return 0;
      setCom_characteristic->setValue(String(dRoute) + "|" + dMessage);
      if (!setCom_characteristic->notify()) ESP_LOGW("BLE", "Notify failed for setCom_characteristic");
      return 1;
}

int bleApplicationComSend(const char* dRoute, String dMessage){
      if(Applications::bleCentralContd == false) return 0;
      appCom_characteristic->setValue(String(dRoute) + "|" + dMessage);
      if (!appCom_characteristic->notify()) ESP_LOGW("BLE", "Notify failed for appCom_characteristic");
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
  Services *thisService = (Services *)dService;
  bleCom_Data_Trans_t qMessage;
  DeserializationError dError;
  String specify_Settings;
  uint16_t dSetCategory = 0;

  while (xQueueReceive(setCom_queue, &qMessage, pdMS_TO_TICKS(500))){
    printf("Settings Payload is: %s\n", (char*) qMessage.payload);
    
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
    default: statusBarNotif.scroll_This_Text("COMMAND IS NOT RECOGNISED.", YELLOW);
      break;
    }

    vTaskDelay(1); // Or yield inside the while loop
    free(qMessage.payload); // Free the allocated memory
    //qMessage.payload = NULL; // Set pointer to NULL to avoid dangling pointer
  }
  
  kill_This_Service(thisService);
}
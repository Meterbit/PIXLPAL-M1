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

#define BLE_COMM_QUEUE_SIZE 6

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


// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define PXP_BLE_SERVICE_UUID "73f3ee85-31f7-4f5a-bd9c-3b469dff65c7"
#define SETCOM_CHARACTERISTIC_UUID "472a6244-3bb8-4a7e-a107-4b47dea92bc3"
#define APPCOM_CHARACTERISTIC_UUID "c8f1eead-48b0-449d-accb-5fdb87c4b566"

EXT_RAM_BSS_ATTR Mtb_Services *mtb_Sett_BleComm_Parser_Sv = new Mtb_Services(ble_SetCom_Parse_Task, &ble_SetCom_Parser_Task_Handle, "bleSetCom_parser_task", 4096, 4); // THIS FUNCTIONS CANNOT BE AN PSRAM MEMORY BECAUSE THEY MIGHT ATTEMPT TO WRITE THE ONBOARD FLASH
EXT_RAM_BSS_ATTR Mtb_Service_With_Fns *mtb_App_BleComm_Parser_Sv = new Mtb_Service_With_Fns(ble_AppCom_Parse_Task, &ble_AppCom_Parser_Task_Handle, "bleAppCom_Parser_task", 4096, 4); // THIS FUNCTIONS CANNOT BE AN PSRAM MEMORY BECAUSE THEY MIGHT ATTEMPT TO WRITE THE ONBOARD FLASH

class MyServerCallbacks : public NimBLEServerCallbacks{
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo){
    // Request MTU
    NimBLEDevice::setMTU(512); // Request MTU size of 512
    isDisconnected = false;
    Mtb_Applications::bleCentralContd = true;
    connHandle = connInfo.getConnHandle();
    mtb_Show_Status_Bar_Icon({"/batIcons/phoneCont.png", 18, 1});
    mtb_Read_Nvs_Struct("pxpBleDevName", pxp_BLE_Name, sizeof(pxp_BLE_Name));
    mtb_Current_Ble_Device(pxp_BLE_Name);
    //ESP_LOGI(TAG, "Connected\n");
  };

  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo, int reason){
    //ESP_LOGI(TAG, "Disconnection detected.\n");
    isDisconnected = true;
    Mtb_Applications::bleCentralContd = false;
    mtb_Show_Status_Bar_Icon({"/batIcons/btOn.png", 18, 1});
    // Start advertising
    NimBLEDevice::startAdvertising(); // Start advertising
  }
};

class CharacteristicsCallbacks : public NimBLECharacteristicCallbacks{
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo){

    ESP_LOGI(TAG, "Value Written: %s \n", pCharacteristic->getValue().c_str());

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

  if(setCom_queue == NULL) setCom_queue = xQueueCreate(BLE_COMM_QUEUE_SIZE, sizeof(mtb_BleCom_Data_Trans_t));     // REVISIT -> Potential memory savings by putting queue in PSRAM.
  if(appCom_queue == NULL) appCom_queue = xQueueCreate(BLE_COMM_QUEUE_SIZE, sizeof(mtb_BleCom_Data_Trans_t));     // REVISIT -> Potential memory savings by putting queue in PSRAM.

    // Create the BLE Device
    mtb_Read_Nvs_Struct("pxpBleDevName", pxp_BLE_Name, sizeof(pxp_BLE_Name));
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

    mtb_Read_Nvs_Struct("pxpBleDevName", pxp_BLE_Name, sizeof(pxp_BLE_Name));
    pAdvertising->setName(pxp_BLE_Name); // Set the device name
    NimBLEDevice::startAdvertising(); // Start advertising

    setCom_characteristic->setValue("Setting Xter Ready.");
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

int bleSettingsComSend(const char* dRoute, String dMessage){
      if(Mtb_Applications::bleCentralContd == false) return 0;
      setCom_characteristic->setValue(String(dRoute) + "|" + dMessage);
      if (!setCom_characteristic->notify()) ESP_LOGW("BLE", "Notify failed for setCom_characteristic");
      return 1;
}

int bleApplicationComSend(const char* dRoute, String dMessage){
      if(Mtb_Applications::bleCentralContd == false) return 0;
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
  Mtb_Services *thisService = (Mtb_Services *)dService;
  mtb_BleCom_Data_Trans_t qMessage;
  DeserializationError dError;
  String specify_Settings;
  uint16_t dSetCategory = 0;

  while (xQueueReceive(setCom_queue, &qMessage, pdMS_TO_TICKS(500))){
    ESP_LOGI(TAG, "Settings Payload is: %s\n", (char*) qMessage.payload);
    
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
        String specify_Application = dInstruction.substring(0, charIndex);  // copy up to the target character
        String dJsonPayload = dInstruction.substring(++charIndex);

        // ESP_LOGI(TAG, "The specific App is: %s\n", specify_Application.c_str());
        // ESP_LOGI(TAG, "The dPayload for App is: %s\n", dPayload.c_str());
 
        dAppGen = getIntegerAtIndex(specify_Application, 0);
        dAppSpe = getIntegerAtIndex(specify_Application, 1);

        //ESP_LOGI(TAG, "The dAppGen is: %d\n", dAppGen);
        //ESP_LOGI(TAG, "The dAppSpe is: %d\n", dAppSpe);

        if (dAppGen == currentApp.GenApp && dAppSpe == currentApp.SpeApp){

            dError = deserializeJson(dCommand, dJsonPayload);
            if(dError.code() == dError.Ok) dCmd_num = dCommand["app_command"];
            else dCmd_num = 0xFFFF;

            switch (dCmd_num){
            case 0: if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[0] != nullptr) mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[0](dCommand);
                break;
            case 1: if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[1] != nullptr) mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[1](dCommand);
                break;
            case 2: if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[2] != nullptr) mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[2](dCommand);
                break;
            case 3: if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[3] != nullptr) mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[3](dCommand);
                break;
            case 4: if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[4] != nullptr) mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[4](dCommand);
              break;
            case 5: if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[5] != nullptr) mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[5](dCommand);
                break;
            case 6: if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[6] != nullptr) mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[6](dCommand);
                break;
            case 7: if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[7] != nullptr) mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[7](dCommand);
                break;
            case 8: if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[8] != nullptr) mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[8](dCommand);
                break;
            case 9: if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[9] != nullptr) mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[9](dCommand);
                break;
            case 10: if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[10] != nullptr) mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[10](dCommand);
                break;
            case 11: if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[11] != nullptr) mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[11](dCommand);
                break;
            // case 12: if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[12] != nullptr) mtb_App_BleComm_Parser_Sv.bleAppComServiceFns[12](dCommand);;     // These are for extras or future upgrades.
            //     break;
            // case 13:  if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[13] != nullptr) mtb_App_BleComm_Parser_Sv.bleAppComServiceFns[13](dCommand);
            //     break;
            // case 14:  if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[4] != nullptr) mtb_App_BleComm_Parser_Sv.bleAppComServiceFns[14](dCommand);
            //     break;
            // case 15:  if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[15] != nullptr) mtb_App_BleComm_Parser_Sv.bleAppComServiceFns[15](dCommand);
            //     break;
            // case 16:  if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[16] != nullptr) mtb_App_BleComm_Parser_Sv.bleAppComServiceFns[16](dCommand);
            //     break;
            // case 17:  if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[17] != nullptr) mtb_App_BleComm_Parser_Sv.bleAppComServiceFns[17](dCommand);
            //     break;
            // case 18:  if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[18] != nullptr) mtb_App_BleComm_Parser_Sv.bleAppComServiceFns[18](dCommand);
            //     break;
            // case 19:  if(mtb_App_BleComm_Parser_Sv->bleAppComServiceFns[19] != nullptr) mtb_App_BleComm_Parser_Sv.bleAppComServiceFns[19](dCommand);
            //     break;
            case 255:
                statusBarNotif.mtb_Scroll_This_Text("APP IS ALREADY ACTIVE", CYAN);
                bleApplicationComSend(specify_Application.c_str(), "{\"pxp_command\": 255}");
                break;
            default: statusBarNotif.mtb_Scroll_This_Text("ERROR: ASSESS COMMAND PARAMETERS", YELLOW);
                break;
            }
        }else{
            dError = deserializeJson(dCommand, dJsonPayload);
            dCmd_num = dCommand["app_command"];

            if (dError.code() == dError.Ok && dCmd_num == 0xFF){
                currentApp.GenApp = getIntegerAtIndex(specify_Application, 0);
                currentApp.SpeApp = getIntegerAtIndex(specify_Application, 1);
                mtb_Write_Nvs_Struct("currentApp", &currentApp, sizeof(Mtb_CurrentApp_t));
                mtb_General_App_Lunch(currentApp);
                bleApplicationComSend(specify_Application.c_str(), "{\"pxp_command\": 253}");
            }else{
                bleApplicationComSend(specify_Application.c_str(), "{\"pxp_command\": 254}");
                statusBarNotif.mtb_Scroll_This_Text("TAP 'LAUNCH' TO START APP", MAGENTA);
            } 
        }
    vTaskDelay(1);
    free(qMessage.payload);
    //qMessage.payload = NULL; // Set pointer to NULL to avoid dangling pointer    
  }
  mtb_Delete_This_Service(thisService);
}
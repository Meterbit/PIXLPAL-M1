#include <Arduino.h>
#include <HTTPClient.h>
#include "mtb_github.h"
#include "mtb_text_scroll.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "mtb_nvs.h"
#include "mtb_engine.h"

struct AppleNotification_Data_t {
uint8_t notification = 0; 
};

AppleNotification_Data_t appleNotificationInfo; // REVISIT -> Move into  PSRAM by making the variable memory dynamic.

EXT_RAM_BSS_ATTR TaskHandle_t appleNotification_Task_H = NULL;
void appleNotifications_App_Task(void *);
// supporting functions

// bluetooth functions
void cancelAppLaunch(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *apple_Notifications_App = new Mtb_Applications_StatusBar(appleNotifications_App_Task, &appleNotification_Task_H, "apple Notif", {7,0}, 4096);

void appleNotifications_App_Task(void* dApplication){
  Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
  THIS_APP->mtb_App_Set_Ble_Comm_Sv_Fns(cancelAppLaunch);
  THIS_APP->mtb_App_Init();
  //************************************************************************************ */
  mtb_Read_Nvs_Struct("appleNotif", &appleNotificationInfo, sizeof(AppleNotification_Data_t));

while (THIS_APP_IS_ACTIVE == pdTRUE) {

    while ((Mtb_Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);


    while (THIS_APP_IS_ACTIVE == pdTRUE) {}

}

  mtb_Delete_This_App(THIS_APP);
}


void cancelAppLaunch(JsonDocument& dCommand){
    String location = dCommand["duration"];
    mtb_Write_Nvs_Struct("appleNotif", &appleNotificationInfo, sizeof(AppleNotification_Data_t));
}




// #include "NimBLEDevice.h"
// #include "driver/uart.h"
// #include <cstdio>
// #include <cstring>

// // ============================================================
// // ANCS UUIDs
// // ============================================================
// static NimBLEUUID ANCS_SERVICE_UUID("7905F431-B5CE-4E99-A40F-4B1E122D00D0");
// static NimBLEUUID NOTIFICATION_SOURCE_UUID("9FBF120D-6301-42D9-8C58-25E699A21DBD");
// static NimBLEUUID CONTROL_POINT_UUID("69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9");
// static NimBLEUUID DATA_SOURCE_UUID("22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB");

// // ============================================================
// // Globals
// // ============================================================
// static NimBLEServer* g_server = nullptr;
// static NimBLEClient* g_peerClient = nullptr;

// static NimBLERemoteService* g_ancsService = nullptr;
// static NimBLERemoteCharacteristic* g_notificationSourceChar = nullptr;
// static NimBLERemoteCharacteristic* g_controlPointChar = nullptr;
// static NimBLERemoteCharacteristic* g_dataSourceChar = nullptr;

// static volatile bool g_linkConnected = false;
// static volatile bool g_linkEncrypted = false;
// static volatile bool g_ancsReady = false;
// static volatile bool g_pendingNotification = false;
// static volatile bool g_incomingCall = false;

// static uint8_t g_latestNotificationUid[4] = {0};
// static uint8_t g_acceptCall = 0;

// // ============================================================
// // UART
// // ============================================================
// static void initUart()
// {
//     uart_config_t uartConfig{};
//     uartConfig.baud_rate = 115200;
//     uartConfig.data_bits = UART_DATA_8_BITS;
//     uartConfig.parity = UART_PARITY_DISABLE;
//     uartConfig.stop_bits = UART_STOP_BITS_1;
//     uartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
//     uartConfig.source_clk = UART_SCLK_DEFAULT;

//     uart_driver_install(UART_NUM_0, 256, 0, 0, nullptr, 0);
//     uart_param_config(UART_NUM_0, &uartConfig);
// }

// // ============================================================
// // Helpers
// // ============================================================
// static void clearAncsState()
// {
//     g_peerClient = nullptr;
//     g_ancsService = nullptr;
//     g_notificationSourceChar = nullptr;
//     g_controlPointChar = nullptr;
//     g_dataSourceChar = nullptr;

//     g_linkConnected = false;
//     g_linkEncrypted = false;
//     g_ancsReady = false;
//     g_pendingNotification = false;
//     g_incomingCall = false;

//     g_latestNotificationUid[0] = 0;
//     g_latestNotificationUid[1] = 0;
//     g_latestNotificationUid[2] = 0;
//     g_latestNotificationUid[3] = 0;
// }

// static void printHex(const uint8_t* data, size_t length)
// {
//     for (size_t i = 0; i < length; ++i) {
//         printf("%02X ", data[i]);
//     }
//     printf("\n");
// }

// // ============================================================
// // ANCS notify callbacks
// // ============================================================
// static void dataSourceNotifyCallback(NimBLERemoteCharacteristic* pCharacteristic,
//                                      uint8_t* pData,
//                                      size_t length,
//                                      bool isNotify)
// {
//     printf("ANCS Data Source (%u bytes): ", (unsigned)length);
//     printHex(pData, length);

//     // Optional: decode attribute stream here.
//     // For now just print raw data.
//     printf("ANCS Data Source ASCII: ");
//     for (size_t i = 0; i < length; ++i) {
//         char c = static_cast<char>(pData[i]);
//         if (c >= 32 && c <= 126) {
//             printf("%c", c);
//         } else {
//             printf(".");
//         }
//     }
//     printf("\n");
// }

// static void notificationSourceNotifyCallback(NimBLERemoteCharacteristic* pCharacteristic,
//                                              uint8_t* pData,
//                                              size_t length,
//                                              bool isNotify)
// {
//     if (length < 8) {
//         printf("Notification Source payload too short: %u\n", (unsigned)length);
//         return;
//     }

//     // EventID, EventFlags, CategoryID, CategoryCount, NotificationUID[4]
//     const uint8_t eventId = pData[0];
//     const uint8_t categoryId = pData[2];

//     g_latestNotificationUid[0] = pData[4];
//     g_latestNotificationUid[1] = pData[5];
//     g_latestNotificationUid[2] = pData[6];
//     g_latestNotificationUid[3] = pData[7];

//     printf("Notification Source: ");
//     printHex(pData, length);

//     if (eventId == 0) {
//         printf("New notification\n");
//     } else if (eventId == 1) {
//         printf("Modified notification\n");
//     } else if (eventId == 2) {
//         printf("Removed notification\n");
//         if (categoryId == 1) {
//             g_incomingCall = false;
//         }
//         return;
//     } else {
//         printf("Unknown EventID=%u\n", eventId);
//     }

//     switch (categoryId) {
//         case 0:  printf("Category: Other\n"); break;
//         case 1:  printf("Category: Incoming Call\n"); g_incomingCall = true; break;
//         case 2:  printf("Category: Missed Call\n"); break;
//         case 3:  printf("Category: Voicemail\n"); break;
//         case 4:  printf("Category: Social\n"); break;
//         case 5:  printf("Category: Schedule\n"); break;
//         case 6:  printf("Category: Email\n"); break;
//         case 7:  printf("Category: News\n"); break;
//         case 8:  printf("Category: Health\n"); break;
//         case 9:  printf("Category: Business\n"); break;
//         case 10: printf("Category: Location\n"); break;
//         case 11: printf("Category: Entertainment\n"); break;
//         default: printf("Category: Unknown (%u)\n", categoryId); break;
//     }

//     g_pendingNotification = true;
// }

// // ============================================================
// // Server callbacks
// // ============================================================
// class MyServerCallbacks : public NimBLEServerCallbacks
// {
// void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override
// {
//     printf("Peer connected: %s\n", connInfo.getAddress().toString().c_str());
//     g_linkConnected = true;

//     g_peerClient = pServer->getClient(connInfo);
//     if (g_peerClient == nullptr) {
//         printf("Failed to create peer client from connection handle\n");
//     } else {
//         printf("Peer client created from server connection\n");
//     }

//     int rc = 0;
//     bool secStarted = NimBLEDevice::startSecurity(connInfo.getConnHandle(), &rc);
//     printf("startSecurity = %d, rc = %d\n", secStarted, rc);
// }

// void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override
//     {
//         printf("Peer disconnected: %s reason=%d\n",
//                connInfo.getAddress().toString().c_str(), reason);

//         clearAncsState();
//     }

// uint32_t onPassKeyDisplay() override
//     {
//         const uint32_t passkey = 123456;
//         printf("Display passkey: %06lu\n", (unsigned long)passkey);
//         return passkey;
//     }

// void onPassKeyEntry(NimBLEConnInfo& connInfo) override
//     {
//         printf("Passkey entry requested for: %s\n",
//                connInfo.getAddress().toString().c_str());

//         // If you need to inject a passkey programmatically, do it here:
//         // NimBLEDevice::injectPassKey(connInfo, 123456);
//     }

//     void onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pin) override {
//         printf("Confirm passkey %06lu for %s\n",
//                (unsigned long)pin,
//                connInfo.getAddress().toString().c_str());

//         // Auto-accept numeric comparison
//         NimBLEDevice::injectConfirmPasskey(connInfo, true);
//     }

//     void onAuthenticationComplete(NimBLEConnInfo& connInfo) override
//     {
//         printf("Authentication complete. Encrypted=%d Bonded=%d Authenticated=%d\n",
//                connInfo.isEncrypted(),
//                connInfo.isBonded(),
//                connInfo.isAuthenticated());

//         g_linkEncrypted = connInfo.isEncrypted();

//         if (!g_linkEncrypted) {
//             printf("Link is not encrypted; ANCS access will fail\n");
//         }
//     }
// };

// static MyServerCallbacks g_serverCallbacks;

// // ============================================================
// // Discover + subscribe to ANCS on iPhone side
// // ============================================================
// static bool discoverAncs()
// {
//     if (g_peerClient == nullptr) {
//         printf("discoverAncs: peer client is null\n");
//         return false;
//     }

//     if (!g_peerClient->isConnected()) {
//         printf("discoverAncs: peer client not connected\n");
//         return false;
//     }

//     if (!g_linkEncrypted) {
//         printf("discoverAncs: link not encrypted yet\n");
//         return false;
//     }

//     g_ancsService = g_peerClient->getService(ANCS_SERVICE_UUID);
//     if (g_ancsService == nullptr) {
//         printf("ANCS service not found on peer\n");
//         return false;
//     }

//     g_notificationSourceChar = g_ancsService->getCharacteristic(NOTIFICATION_SOURCE_UUID);
//     g_controlPointChar       = g_ancsService->getCharacteristic(CONTROL_POINT_UUID);
//     g_dataSourceChar         = g_ancsService->getCharacteristic(DATA_SOURCE_UUID);

//     if (g_notificationSourceChar == nullptr) {
//         printf("Notification Source characteristic not found\n");
//         return false;
//     }

//     if (g_controlPointChar == nullptr) {
//         printf("Control Point characteristic not found\n");
//         return false;
//     }

//     if (g_dataSourceChar == nullptr) {
//         printf("Data Source characteristic not found\n");
//         return false;
//     }

//     printf("ANCS characteristics discovered successfully\n");
//     return true;
// }

// static bool subscribeAncs()
// {
//     if (g_notificationSourceChar == nullptr || g_dataSourceChar == nullptr) {
//         return false;
//     }

//     bool ok1 = g_notificationSourceChar->subscribe(true, notificationSourceNotifyCallback, true);
//     bool ok2 = g_dataSourceChar->subscribe(true, dataSourceNotifyCallback, true);

//     printf("Subscribe NotificationSource=%d DataSource=%d\n", ok1, ok2);

//     if (ok1 && ok2) {
//         g_ancsReady = true;
//         return true;
//     }

//     return false;
// }

// // ============================================================
// // ANCS command helpers
// // ============================================================

// // Request Title, Message, and Date for the most recent UID.
// // CommandID = 0 (Get Notification Attributes)
// static void requestNotificationAttributes()
// {
//     if (g_controlPointChar == nullptr) {
//         return;
//     }

//     // Format:
//     // [0]   CommandID = 0
//     // [1:4] NotificationUID
//     // [5]   AttributeID Title = 1
//     // [6:7] MaxLen = 64
//     // [8]   AttributeID Message = 3
//     // [9:10] MaxLen = 128
//     // [11]  AttributeID Date = 5
//     uint8_t req[] = {
//         0x00,
//         g_latestNotificationUid[0],
//         g_latestNotificationUid[1],
//         g_latestNotificationUid[2],
//         g_latestNotificationUid[3],
//         0x01, 0x40, 0x00,
//         0x03, 0x80, 0x00,
//         0x05
//     };

//     printf("Requesting notification attributes for UID: %02X %02X %02X %02X\n",
//            g_latestNotificationUid[0],
//            g_latestNotificationUid[1],
//            g_latestNotificationUid[2],
//            g_latestNotificationUid[3]);

//     bool ok = g_controlPointChar->writeValue(req, sizeof(req), true);
//     printf("Control Point write (Get Notification Attributes) = %d\n", ok);
// }

// // Perform positive/negative action for incoming call.
// // CommandID = 2 (Perform Notification Action)
// // ActionID 0 = Positive
// // ActionID 1 = Negative
// static void handleIncomingCallAction()
// {
//     if (!g_incomingCall || g_controlPointChar == nullptr) {
//         return;
//     }

//     int bytesRead = uart_read_bytes(UART_NUM_0, &g_acceptCall, 1, 0);
//     if (bytesRead <= 0) {
//         return;
//     }

//     printf("UART call action input: %c\n", (char)g_acceptCall);

//     if (g_acceptCall == '1') {
//         uint8_t cmd[] = {
//             0x02,
//             g_latestNotificationUid[0],
//             g_latestNotificationUid[1],
//             g_latestNotificationUid[2],
//             g_latestNotificationUid[3],
//             0x00
//         };

//         bool ok = g_controlPointChar->writeValue(cmd, sizeof(cmd), true);
//         printf("Positive call action sent: %d\n", ok);
//         g_acceptCall = 0;
//     } else if (g_acceptCall == '0') {
//         uint8_t cmd[] = {
//             0x02,
//             g_latestNotificationUid[0],
//             g_latestNotificationUid[1],
//             g_latestNotificationUid[2],
//             g_latestNotificationUid[3],
//             0x01
//         };

//         bool ok = g_controlPointChar->writeValue(cmd, sizeof(cmd), true);
//         printf("Negative call action sent: %d\n", ok);
//         g_acceptCall = 0;
//         g_incomingCall = false;
//     } else {
//         g_acceptCall = 0;
//     }
// }

// // ============================================================
// // Advertising setup
// // ============================================================

// // AD type 0x15 = Service Solicitation, 128-bit UUID
// // For ANCS UUID in little-endian:
// // 7905F431-B5CE-4E99-A40F-4B1E122D00D0
// // => D0 00 2D 12 1E 4B 0F A4 99 4E CE B5 31 F4 05 79
// static bool setupAdvertising()
// {
//     NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
//     if (pAdvertising == nullptr) {
//         printf("Failed to get advertising object\n");
//         return false;
//     }

//     NimBLEAdvertisementData advData;
//     NimBLEAdvertisementData scanData;

//     // Standard BLE flags: General Discoverable + BR/EDR not supported
//     advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);

//     // Add ANCS service solicitation AD structure manually
//     // [len=17][type=0x15][16 bytes UUID little-endian]
//     const uint8_t solicitationField[] = {
//         17, 0x15,
//         0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4,
//         0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79
//     };

//     if (!advData.addData(solicitationField, sizeof(solicitationField))) {
//         printf("Failed to add ANCS solicitation field\n");
//         return false;
//     }

//     advData.addTxPower();

//     // Put the local name in scan response so iPhone scanners/settings can show it.
//     scanData.setName("ESP32-ANCS", true);

//     pAdvertising->setAdvertisementData(advData);
//     pAdvertising->setScanResponseData(scanData);
//     pAdvertising->setMinInterval(160);  // 100 ms
//     pAdvertising->setMaxInterval(240);  // 150 ms

//     bool ok = pAdvertising->start();
//     printf("Advertising start result = %d\n", ok);

//     return ok;
// }

// // ============================================================
// // Main
// // ============================================================
// extern "C" void app_main(void)
// {
//     initUart();
//     clearAncsState();

//     printf("Starting ESP32 ANCS accessory base...\n");

//     // Initialize NimBLE.
//     NimBLEDevice::init("ESP32-ANCS");

//     // Security: bonding + MITM + secure connections
//     NimBLEDevice::setSecurityAuth(true, true, true);
//     NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO);

//     // Optional power boost
//     NimBLEDevice::setPower(9);

//     // Create local server (peripheral role).
//     g_server = NimBLEDevice::createServer();
//     g_server->setCallbacks(&g_serverCallbacks, false);
//     g_server->advertiseOnDisconnect(true);

//     // Start a tiny local service so the GATT server is up.
//     // This local service is not ANCS; the iPhone owns ANCS.
//     NimBLEService* pDummySvc = g_server->createService("180A");
//     pDummySvc->start();

//     if (!setupAdvertising()) {
//         printf("Advertising setup failed\n");
//     } else {
//         printf("Advertising for ANCS pairing/discovery\n");
//     }

//     while (true) {
//         if (g_linkConnected && g_linkEncrypted && !g_ancsReady) {
//             printf("Trying ANCS discovery...\n");

//             if (discoverAncs()) {
//                 if (subscribeAncs()) {
//                     printf("ANCS is ready\n");
//                 } else {
//                     printf("ANCS subscription failed\n");
//                 }
//             }
//         }

//         if (g_ancsReady && g_pendingNotification) {
//             g_pendingNotification = false;
//             requestNotificationAttributes();
//         }

//         if (g_ancsReady && g_incomingCall) {
//             handleIncomingCallAction();
//         }

//         vTaskDelay(pdMS_TO_TICKS(50));
//     }
// }
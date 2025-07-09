#include "mtbApps.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "twelveDataFX.h"

TwelveDataFX_t twelveDataFX = {
    "EUR/USD",
    "",
    60
};

EXT_RAM_BSS_ATTR TaskHandle_t twelveData_Task_H = NULL;
void twelveData_App_Task(void *);

// BLE command functions
void setTwelveDataPair(DynamicJsonDocument &);
void setTwelveDataInterval(DynamicJsonDocument &);
void setTwelveDataAPIKey(DynamicJsonDocument &);

EXT_RAM_BSS_ATTR Applications_StatusBar *twelveData_App = new Applications_StatusBar(twelveData_App_Task, &twelveData_Task_H, "TwelveData FX", 8192);

void twelveData_App_Task(void *dApplication){
    Applications *thisApp = (Applications *)dApplication;
    thisApp->app_EncoderFn_ptr = brightnessControl;
    ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(setTwelveDataPair, setTwelveDataInterval, setTwelveDataAPIKey);
    appsInitialization(thisApp, statusBarClock_Sv);

    read_struct_from_nvs("twelveDataFX", &twelveDataFX, sizeof(TwelveDataFX_t));

    StaticJsonDocument<256> doc;
    char apiUrl[256];
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    CentreText_t pairTxt(63, 16, Terminal6x8, YELLOW);
    CentreText_t priceTxt(63, 32, Terminal6x8, CYAN);

    while(THIS_APP_IS_ACTIVE == pdTRUE){
        while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);

        snprintf(apiUrl, sizeof(apiUrl), "https://api.twelvedata.com/price?symbol=%s&apikey=%s", twelveDataFX.pair, twelveDataFX.apiToken);
        http.begin(client, apiUrl);
        int httpCode = http.GET();
        if(httpCode == 200){
            String payload = http.getString();
            DeserializationError err = deserializeJson(doc, payload);
            if(!err){
                const char* price = doc["price"];
                if(price){
                    pairTxt.writeString(String(twelveDataFX.pair));
                    priceTxt.writeString(String(price));
                }
            }
        } else {
            printf("HTTP GET failed: %d\n", httpCode);
        }
        http.end();

        int32_t waitMs = (twelveDataFX.updateInterval > 0 ? twelveDataFX.updateInterval*1000 : 30000);
        for(int32_t t=0; t<waitMs && THIS_APP_IS_ACTIVE; t+=1000) delay(1000);
    }
    kill_This_App(thisApp);
}

void setTwelveDataPair(DynamicJsonDocument &dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    const char *pair = dCommand["pair"];
    if(pair){
        strncpy(twelveDataFX.pair, pair, sizeof(twelveDataFX.pair)-1);
        write_struct_to_nvs("twelveDataFX", &twelveDataFX, sizeof(TwelveDataFX_t));
    }
    ble_Application_Command_Respond_Success(twelveDataAppRoute, cmdNumber, pdPASS);
}

void setTwelveDataInterval(DynamicJsonDocument &dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    int16_t interval = dCommand["dInterval"];
    if(interval > 0){
        twelveDataFX.updateInterval = interval;
        write_struct_to_nvs("twelveDataFX", &twelveDataFX, sizeof(TwelveDataFX_t));
    }
    ble_Application_Command_Respond_Success(twelveDataAppRoute, cmdNumber, pdPASS);
}

void setTwelveDataAPIKey(DynamicJsonDocument &dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String key = dCommand["api_key"];
    strncpy(twelveDataFX.apiToken, key.c_str(), sizeof(twelveDataFX.apiToken)-1);
    write_struct_to_nvs("twelveDataFX", &twelveDataFX, sizeof(TwelveDataFX_t));
    ble_Application_Command_Respond_Success(twelveDataAppRoute, cmdNumber, pdPASS);
}


#include "mtbApps.h"
#include <HTTPClient.h>
#include "mtbGithubStorage.h"
#include "scrollMsgs.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include "currencyExchange.h"

#define MAX_CurrencyS 100

// Default Currency
Currency_Stat_t currentCurrencies = {

};

EXT_RAM_BSS_ATTR SemaphoreHandle_t changeDispCurrency_Sem = NULL;
EXT_RAM_BSS_ATTR TimerHandle_t currencyChangeTimer_H = NULL;
int8_t currencyIterate = 0;

String currencySymbols[MAX_CurrencyS];
int currencyCount = 0;
static const char currencySymbolsFilePath[] = "/currency/currency.csv";

EXT_RAM_BSS_ATTR TaskHandle_t currencyExchange_Task_H = NULL;
void currencyExchange_App_Task(void *);
void readCurrencySymbols(const char *filename, String currencySymbols[], int &count, const int maxSymbols);

void buttonChangeDisplayCurrency(button_event_t button_Data);
void currencyChange_TimerCallback(TimerHandle_t);

void showParticularCurrencies(DynamicJsonDocument&);
void add_RemoveCurrencySymbol(DynamicJsonDocument&);
void setcurrencyChangeInterval(DynamicJsonDocument&);
void saveCurrencyAPI_key(DynamicJsonDocument&);

EXT_RAM_BSS_ATTR Applications_StatusBar *currencyExchange_App = new Applications_StatusBar(currencyExchange_App_Task, &currencyExchange_Task_H, "currencyEx", 10240);

void currencyExchange_App_Task(void* dApplication){
  Applications *thisApp = (Applications *)dApplication;
  thisApp->app_EncoderFn_ptr = brightnessControl;
  thisApp->app_ButtonFn_ptr = buttonChangeDisplayCurrency;
  ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(showParticularCurrencies, add_RemoveCurrencySymbol, setcurrencyChangeInterval, saveCurrencyAPI_key);
  appsInitialization(thisApp);
  //************************************************************************************ */
  read_struct_from_nvs("currencyStat", &currentCurrencies, sizeof(Currency_Stat_t));
  time_t present = 0;
  if(changeDispCurrency_Sem == NULL) changeDispCurrency_Sem = xSemaphoreCreateBinary();
  if(currencyChangeTimer_H == NULL) currencyChangeTimer_H = xTimerCreate("CurrencyChange Timer", pdMS_TO_TICKS(currentCurrencies.currencyChangeInterval > 0 ? (currentCurrencies.currencyChangeInterval * 1000) : (30 * 1000)), true, NULL, currencyChange_TimerCallback);
  StaticJsonDocument<3000> doc;
  ScrollText_t moreCurrencyData (63, 54, 63, WHITE, 25, 1, Terminal6x8);

  FixedText_t CurrencyID1_tag(63, 4, Terminal6x8, CYAN);
  FixedText_t current_price_tag(63, 16, Terminal6x8, YELLOW);
  FixedText_t priceChangePercent_tag(63, 28, Terminal6x8, YELLOW_GREEN);    
  FixedText_t cPrice_diff_tag(63, 40, Terminal6x8, GREEN);

  CurrencyID1_tag.writeString("STK:");
  current_price_tag.writeString("PRC:");
  cPrice_diff_tag.writeString("DPR:");
  priceChangePercent_tag.writeString("G/L:");

  FixedText_t CurrencyID1_txt(88, 4, Terminal6x8, CYAN);
  FixedText_t current_price_txt(88, 16, Terminal6x8, YELLOW);
  FixedText_t priceChangePercent_txt(88, 28, Terminal6x8, YELLOW_GREEN);    
  FixedText_t cPrice_diff_txt(88, 40, Terminal6x8, GREEN);
//##############################################################################################################
    
    // Read Currency symbols from the CSV file
    readCurrencySymbols(currencySymbolsFilePath, currencySymbols, currencyCount, MAX_CurrencyS);
    
    //printf("Found %d Currency symbols:\n", currencyCount);
    // for (int i = 0; i < currencyCount; i++) {
    //     printf("%s\n",currencySymbols[i].c_str());
    // }

    // If no symbols are found, write default symbols to the file
    if (currencyCount == 0) {
        File file = LittleFS.open(currencySymbolsFilePath, "w");
        if (file) {
            String defaultSymbols = "AAPL,NVDA,AMZN,TSLA,MSFT,GOOG";
            file.print(defaultSymbols);
            file.close();
            printf("Default Currency symbols written to file.\n");

            // Read the symbols again after writing defaults
            readCurrencySymbols(currencySymbolsFilePath, currencySymbols, currencyCount, MAX_CurrencyS);
            printf("After writing defaults, found %d Currency symbols:\n", currencyCount);
            for (int i = 0; i < currencyCount; i++) {
                printf("%s\n",currencySymbols[i].c_str());
            }
        } else {
            printf("Failed to write default symbols to file.\n");
        }
    } 
    
    if(currentCurrencies.currencyChangeInterval > 1) xTimerStart(currencyChangeTimer_H, 0);
//##############################################################################################################
//##############################################################################################################
char apiUrl[1000]; // Adjust size as needed
static HTTPClient http;

while (THIS_APP_IS_ACTIVE == pdTRUE){
    snprintf(apiUrl, sizeof(apiUrl), "https://finnhub.io/api/v1/quote?symbol=%s&token=%s", currentCurrencies.currencyID1.c_str(), currentCurrencies.apiToken);
    //printf("OUR FINAL URL IS: %s \n", apiUrl.c_str());
    //******************************************************************************************************* */
    downloadGithubStrgFile("Currencys_Icons/_" + currentCurrencies.currencyID1 +".png", String(currentCurrencies.currencyFilePath));

    while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);
        
        if (http.connected()) { http.end(); } // Cleanup before starting a new request

    //************************************************************************************** */
    while (THIS_APP_IS_ACTIVE == pdTRUE){
        int16_t CurrencyDataRequestTim = 5000;
        http.begin(apiUrl); // Edit your key here
        // http.addHeader("x-rapidapi-key", "783152efb6mshac636e27a8f24bep10fb3ajsn5ae13bc4f722");
        int httpCode = http.GET();
        if (httpCode > 0){
            String payload = http.getString();
            //printf("\n\n Payload: %s \n\n", payload.c_str());

            DeserializationError error = deserializeJson(doc, payload);
            if (error) {
                printf("deserializeJson() failed: %s\n", error.c_str());
                continue; // Skip the rest of the loop iteration if deserialization fails
            }

            // Extract values
            double current_price = doc["c"]; // 63507
            double price_Diff = doc["d"];    // 1254571864931
            double price_change_percentage_24h = doc["dp"];
            present = doc["t"];
            // time(&present);

            double high24 = doc["h"];
            double low24 = doc["l"];
            double openPrice24 = doc["o"];
            double previouClosePrice24 = doc["pc"];

            drawLocalPNG({"/Currencys/CurrencysIcon_1.png", 4, 4});
            CurrencyID1_txt.writeString(currentCurrencies.currencyID1);
            current_price_txt.writeString(String(current_price));
            cPrice_diff_tag.writeColoredString("DPR:", price_Diff < 0 ? RED : GREEN);
            cPrice_diff_txt.writeColoredString(String(price_Diff), price_Diff < 0 ? RED : GREEN);
            priceChangePercent_tag.writeColoredString("G/L:", price_change_percentage_24h < 0 ? ORANGE : YELLOW_GREEN);
            priceChangePercent_txt.writeColoredString(String(price_change_percentage_24h) + "%", price_change_percentage_24h < 0 ? ORANGE : YELLOW_GREEN);

            moreCurrencyData.scroll_This_Text("CURR: " + currentCurrencies.currencyID1, CYAN);
            moreCurrencyData.scroll_This_Text("HIGH: " + String(high24), GREEN);
            moreCurrencyData.scroll_This_Text("LOW: " + String(low24), YELLOW);
            moreCurrencyData.scroll_This_Text("OPEN: " + String(openPrice24), GREEN_CYAN);
            moreCurrencyData.scroll_This_Text("PCPR: " + String(previouClosePrice24), ORANGE);
            moreCurrencyData.scroll_This_Text("TIME: " + unixTimeToReadable(present), YELLOW);

    } else {
        printf("Error on HTTP request: %d \n", httpCode);
    }
        // Close the connection
        http.end();

        while(CurrencyDataRequestTim-->0  && THIS_APP_IS_ACTIVE && xSemaphoreTake(changeDispCurrency_Sem, 0) != pdTRUE) delay(1);
        if(CurrencyDataRequestTim > 0) break;
    }
        moreCurrencyData.scroll_Active(STOP_SCROLL);
}
  kill_This_App(thisApp);
}
//##############################################################################################################

void readCurrencySymbols(const char* filename, String currencySymbols[], int& count, const int maxSymbols){
    
    // Check if the file exists
    if (!LittleFS.exists(filename)) {
        // Create a new file if it doesn't exist
        File file = LittleFS.open(filename, "w");
        if (!file) {
            printf("Failed to create file.\n");
            count = 0;
            return;
        }
        file.close();
        printf("File created successfully.\n");
        count = 0;
        return;
    }
    
    // Open the file for reading
    File file = LittleFS.open(filename, "r");
    if (!file) {
        printf("Failed to open file.\n");
        count = 0;
        return;
    }
    
    // Read the entire content of the file
    String content = file.readString();
    file.close();
    
    // Parse the CSV data to extract Currency symbols
    int start = 0;
    count = 0;
    content.trim();  // Remove leading and trailing whitespace
    while (count < maxSymbols && start < content.length()) {
        int commaIndex = content.indexOf(',', start);
        if (commaIndex == -1) {
            // No more commas; read until the end
            String symbol = content.substring(start);
            symbol.trim();
            if (symbol.length() > 0) {
                currencySymbols[count++] = symbol;
            }
            break;
        } else {
            String symbol = content.substring(start, commaIndex);
            symbol.trim();
            if (symbol.length() > 0) {
                currencySymbols[count++] = symbol;
            }
            start = commaIndex + 1;
        }
    }
}

void buttonChangeDisplayCurrency(button_event_t button_Data){
            switch (button_Data.type){
            case BUTTON_RELEASED:
            break;

            case BUTTON_PRESSED:
                if ((++currencyIterate) < currencyCount) currentCurrencies.currencyID1 = currencySymbols[currencyIterate];
                else currentCurrencies.currencyID1 = currencySymbols[currencyIterate = 0];
                write_struct_to_nvs("CurrencysStat", &currentCurrencies, sizeof(Currency_Stat_t));
                xSemaphoreGive(changeDispCurrency_Sem);
                break;

            case BUTTON_PRESSED_LONG:
            if (xTimerIsTimerActive(currencyChangeTimer_H) == pdTRUE) xTimerStop(currencyChangeTimer_H, 0);
            else xTimerStart(currencyChangeTimer_H, 0);
            break;

            case BUTTON_CLICKED:
            //printf("Button Clicked: %d Times\n",button_Data.count);
            switch (button_Data.count){
            case 1:
                break;
            case 2:
                break;
            case 3:
                break;
            default:
                break;
            }
                break;
            default:
            break;
			}
}

void currencyChange_TimerCallback(TimerHandle_t currencyPrompt){
    if ((++currencyIterate) < currencyCount) currentCurrencies.currencyID1 = currencySymbols[currencyIterate];
    else currentCurrencies.currencyID1 = currencySymbols[currencyIterate = 0];
    write_struct_to_nvs("CurrencysStat", &currentCurrencies, sizeof(Currency_Stat_t));
    xSemaphoreGive(changeDispCurrency_Sem);
}

void showParticularCurrencies(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    const char *currencySymbol = dCommand["stkSymbol"];
    currentCurrencies.currencyID1 = String(currencySymbol);
    write_struct_to_nvs("CurrencysStat", &currentCurrencies, sizeof(Currency_Stat_t));
    xSemaphoreGive(changeDispCurrency_Sem);
    ble_Application_Command_Respond_Success(currencyExchangeAppRoute, cmdNumber, pdPASS);
}

void add_RemoveCurrencySymbol(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String CurrencySymbol = dCommand["stkList"];
    // if(actionCmd) addCurrencySymbol(currencySymbolsFilePath, String(CurrencySymbol));
    // else removeCurrencySymbol(currencySymbolsFilePath, String(CurrencySymbol));

    CurrencySymbol.replace("[", "");
    CurrencySymbol.replace("]", "");

    printf("The Selected Currencys are: %s\n", CurrencySymbol.c_str());

    File file = LittleFS.open("/Currencys/dCurrencys.csv", "w");
        if (file) {
            file.print(CurrencySymbol);
            file.close();
            printf("Default Currency symbols written to file.\n");

            // Read the symbols again after writing defaults
            readCurrencySymbols(currencySymbolsFilePath, currencySymbols, currencyCount, MAX_CurrencyS);
            printf("After writing defaults, found %d Currency symbols:\n", currencyCount);
            for (int i = 0; i < currencyCount; i++) {
                printf("%s\n",currencySymbols[i].c_str());
            }
        } else {
            printf("Failed to write default symbols to file.\n");
            readCurrencySymbols(currencySymbolsFilePath, currencySymbols, currencyCount, MAX_CurrencyS);
        }

    
    // xSemaphoreGive(changeDispCurrency_Sem);
    ble_Application_Command_Respond_Success(currencyExchangeAppRoute, cmdNumber, pdPASS);
}

void setcurrencyChangeInterval(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    uint8_t setCycle = dCommand["cycleCurrencys"];
    int16_t dInterval = dCommand["dInterval"];

    if(setCycle == pdTRUE && xTimerIsTimerActive(currencyChangeTimer_H) == pdFALSE){
        currentCurrencies.currencyChangeInterval = dInterval;
        xTimerStart(currencyChangeTimer_H, 0);
    } else if (setCycle == pdFALSE && xTimerIsTimerActive(currencyChangeTimer_H) == pdTRUE){
        currentCurrencies.currencyChangeInterval = dInterval;
        xTimerStop(currencyChangeTimer_H, 0);
    } else if (setCycle == pdTRUE && xTimerIsTimerActive(currencyChangeTimer_H) == pdTRUE && dInterval != currentCurrencies.currencyChangeInterval){
        currentCurrencies.currencyChangeInterval = dInterval;
        xTimerStop(currencyChangeTimer_H, 0);
        xTimerChangePeriod(currencyChangeTimer_H, pdMS_TO_TICKS(dInterval * 1000), 0);
        xTimerStart(currencyChangeTimer_H, 0);
    }
    write_struct_to_nvs("CurrencysStat", &currentCurrencies, sizeof(Currency_Stat_t));
    ble_Application_Command_Respond_Success(currencyExchangeAppRoute, cmdNumber, pdPASS);
}

// String convertArrayToJson(String arr[], size_t length) {
//     String json = "[";
//     for (size_t i = 0; i < length; i++) {
//         json += "\"";
//         json += arr[i];
//         json += "\"";
//         if (i < length - 1) {
//             json += ",";
//         }
//     }
//     json += "]";
//     return json;
// }

// void loadSavedCurrencys(DynamicJsonDocument& dCommand){
//     uint8_t cmdNumber = dCommand["app_command"];
//     String savedSymbols = "{\"pxp_command\":";
//     savedSymbols += String(cmdNumber) + ",\"savedSymbols\":" + convertArrayToJson(currencySymbols, currencyCount) + "}";
//     bleApplicationComSend(savedSymbols);
// }

void saveCurrencyAPI_key(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String userAPI_Key = dCommand["api_key"];
    strcpy(currentCurrencies.apiToken, userAPI_Key.c_str());
    write_struct_to_nvs("CurrencysStat", &currentCurrencies, sizeof(Currency_Stat_t));
    ble_Application_Command_Respond_Success(currencyExchangeAppRoute, cmdNumber, pdPASS);
}
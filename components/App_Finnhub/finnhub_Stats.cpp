#include "mtbApps.h"
#include <HTTPClient.h>
#include "mtbGithubStorage.h"
#include "scrollMsgs.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include "finnhub_Stats.h"

#define MAX_STOCKS 100

// Default Stock
Stocks_Stat_t currentStocks = {
    "AAPL",
    "USD",
    "/stocks/stocksIcon_1.png",
    "crs3eq1r01qohu2r66c0crs3eq1r01qohu2r66cg",
    1,
    30
};

EXT_RAM_BSS_ATTR SemaphoreHandle_t changeDispStock_Sem = NULL;
EXT_RAM_BSS_ATTR TimerHandle_t stockChangeTimer_H = NULL;
int8_t stockIterate = 0;

String stockSymbols[MAX_STOCKS];
int stockCount = 0;
static const char stockSymbolsFilePath[] = "/stocks/dStocks.csv";

EXT_RAM_BSS_ATTR TaskHandle_t finhubStats_Task_H = NULL;
void finhubStats_App_Task(void *);
void readStockSymbols(const char *filename, String stockSymbols[], int &count, const int maxSymbols);

void buttonChangeDisplayStock(button_event_t button_Data);
void stockChange_TimerCallback(TimerHandle_t);

void showParticularStock(DynamicJsonDocument&);
void add_RemoveStockSymbol(DynamicJsonDocument&);
void setStockChangeInterval(DynamicJsonDocument&);
void saveAPI_key(DynamicJsonDocument&);

EXT_RAM_BSS_ATTR Applications_StatusBar *finnhub_Stats_App = new Applications_StatusBar(finhubStats_App_Task, &finhubStats_Task_H, "Finnhub Stats", 10240);

void finhubStats_App_Task(void* dApplication){
    Applications *thisApp = (Applications *)dApplication;
    thisApp->app_EncoderFn_ptr = brightnessControl;
    thisApp->app_ButtonFn_ptr = buttonChangeDisplayStock;
    ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(showParticularStock, add_RemoveStockSymbol, setStockChangeInterval, saveAPI_key);
    appsInitialization(thisApp, statusBarClock_Sv);
    //************************************************************************************ */
    read_struct_from_nvs("stocksStat", &currentStocks, sizeof(Stocks_Stat_t));
    time_t present = 0;
    if(changeDispStock_Sem == NULL) changeDispStock_Sem = xSemaphoreCreateBinary();
    if(stockChangeTimer_H == NULL) stockChangeTimer_H = xTimerCreate("stockChange Timer", pdMS_TO_TICKS(currentStocks.stockChangeInterval > 0 ? (currentStocks.stockChangeInterval * 1000) : (30 * 1000)), true, NULL, stockChange_TimerCallback);
    StaticJsonDocument<3072> doc;

    FixedText_t stockID_tag(40, 13, Terminal6x8, CYAN);
    FixedText_t current_price_tag(40, 23, Terminal6x8, YELLOW);
    FixedText_t priceChangePercent_tag(40, 33, Terminal6x8, YELLOW_GREEN);    
    FixedText_t cPrice_diff_tag(40, 43, Terminal6x8, GREEN);
    ScrollText_t moreStockData (0, 55, 128, WHITE, 20, 1, Terminal6x8);

    stockID_tag.writeString("STK:");
    current_price_tag.writeString("PRC:");
    cPrice_diff_tag.writeString("DPR:");
    priceChangePercent_tag.writeString("G/L:");

    FixedText_t stockID_txt(67, 13, Terminal6x8, CYAN);
    FixedText_t current_price_txt(67, 23, Terminal6x8, YELLOW);
    FixedText_t priceChangePercent_txt(67, 33, Terminal6x8, YELLOW_GREEN);    
    FixedText_t cPrice_diff_txt(67, 43, Terminal6x8, GREEN);
//##############################################################################################################
    
    // Read stock symbols from the CSV file
    readStockSymbols(stockSymbolsFilePath, stockSymbols, stockCount, MAX_STOCKS);
    
    //printf("Found %d stock symbols:\n", stockCount);
    // for (int i = 0; i < stockCount; i++) {
    //     printf("%s\n",stockSymbols[i].c_str());
    // }

    // If no symbols are found, write default symbols to the file
    if (stockCount == 0) {
        File file = LittleFS.open(stockSymbolsFilePath, "w");
        if (file) {
            String defaultSymbols = "AAPL,NVDA,AMZN,TSLA,MSFT,GOOG";
            file.print(defaultSymbols);
            file.close();
            printf("Default stock symbols written to file.\n");

            // Read the symbols again after writing defaults
            readStockSymbols(stockSymbolsFilePath, stockSymbols, stockCount, MAX_STOCKS);
            printf("After writing defaults, found %d stock symbols:\n", stockCount);
            for (int i = 0; i < stockCount; i++) {
                printf("%s\n",stockSymbols[i].c_str());
            }
        } else {
            printf("Failed to write default symbols to file.\n");
        }
    } 
    
    if(currentStocks.stockChangeInterval > 1) xTimerStart(stockChangeTimer_H, 0);
//##############################################################################################################
//##############################################################################################################
char apiUrl[1000]; // Adjust size as needed
static HTTPClient http;

while (THIS_APP_IS_ACTIVE == pdTRUE){
    snprintf(apiUrl, sizeof(apiUrl), "https://finnhub.io/api/v1/quote?symbol=%s&token=%s", currentStocks.stockID.c_str(), currentStocks.apiToken);
    //printf("OUR FINAL URL IS: %s \n", apiUrl.c_str());
    //******************************************************************************************************* */
    downloadGithubStrgFile("stocks_Icons/_" + currentStocks.stockID +".png", String(currentStocks.stockFilePath));

    while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);
        
        if (http.connected()) { http.end(); } // Cleanup before starting a new request

    //************************************************************************************** */
    while (THIS_APP_IS_ACTIVE == pdTRUE){
        int16_t stockDataRequestTim = 500;
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

            drawLocalPNG({"/stocks/stocksIcon_1.png", 5, 18, 2});

            stockID_txt.writeString(currentStocks.stockID);
            current_price_txt.writeString(String(current_price));
            cPrice_diff_tag.writeColoredString("DPR:", price_Diff < 0 ? RED : GREEN);
            cPrice_diff_txt.writeColoredString(String(price_Diff), price_Diff < 0 ? RED : GREEN);
            priceChangePercent_tag.writeColoredString("G/L:", price_change_percentage_24h < 0 ? ORANGE : YELLOW_GREEN);
            priceChangePercent_txt.writeColoredString(String(price_change_percentage_24h) + "%", price_change_percentage_24h < 0 ? ORANGE : YELLOW_GREEN);

            if(price_Diff < 0) drawLocalPNG({"/gain_lose/lose.png", 104, 20, 1});
            else drawLocalPNG({"/gain_lose/gain.png", 104, 20, 1});

            moreStockData.scroll_This_Text("CURR: " + currentStocks.currency, CYAN);
            moreStockData.scroll_This_Text("HIGH: " + String(high24), GREEN);
            moreStockData.scroll_This_Text("LOW: " + String(low24), YELLOW);
            moreStockData.scroll_This_Text("OPEN: " + String(openPrice24), GREEN_CYAN);
            moreStockData.scroll_This_Text("PCPR: " + String(previouClosePrice24), ORANGE);
            moreStockData.scroll_This_Text("TIME: " + unixTimeToReadable(present), YELLOW);

    } else {
        printf("Error on HTTP request: %d \n", httpCode);
    }
        // Close the connection
        http.end();

        while(stockDataRequestTim-->0  && THIS_APP_IS_ACTIVE && xSemaphoreTake(changeDispStock_Sem, 0) != pdTRUE) delay(10);
        if(stockDataRequestTim > 0) break;
    }
        moreStockData.scroll_Active(STOP_SCROLL);
}
  kill_This_App(thisApp);
}
//##############################################################################################################

void readStockSymbols(const char* filename, String stockSymbols[], int& count, const int maxSymbols){
    
    if (prepareFilePath(filename)){
        printf("File path prepared successfully: %s\n", filename);
    }
    else {
        printf("Failed to prepare file path.\n");
        count = 0;
        return;
    }
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
    
    // Parse the CSV data to extract stock symbols
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
                stockSymbols[count++] = symbol;
            }
            break;
        } else {
            String symbol = content.substring(start, commaIndex);
            symbol.trim();
            if (symbol.length() > 0) {
                stockSymbols[count++] = symbol;
            }
            start = commaIndex + 1;
        }
    }
}

void buttonChangeDisplayStock(button_event_t button_Data){
            switch (button_Data.type){
            case BUTTON_RELEASED:
            break;

            case BUTTON_PRESSED:
                if ((++stockIterate) < stockCount) currentStocks.stockID = stockSymbols[stockIterate];
                else currentStocks.stockID = stockSymbols[stockIterate = 0];
                write_struct_to_nvs("stocksStat", &currentStocks, sizeof(Stocks_Stat_t));
                xSemaphoreGive(changeDispStock_Sem);
                break;

            case BUTTON_PRESSED_LONG:
            if (xTimerIsTimerActive(stockChangeTimer_H) == pdTRUE) xTimerStop(stockChangeTimer_H, 0);
            else xTimerStart(stockChangeTimer_H, 0);
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

void stockChange_TimerCallback(TimerHandle_t stockPrompt){
    if ((++stockIterate) < stockCount) currentStocks.stockID = stockSymbols[stockIterate];
    else currentStocks.stockID = stockSymbols[stockIterate = 0];
    write_struct_to_nvs("stocksStat", &currentStocks, sizeof(Stocks_Stat_t));
    xSemaphoreGive(changeDispStock_Sem);
}

void showParticularStock(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    const char *stockSymbol = dCommand["stkSymbol"];
    currentStocks.stockID = String(stockSymbol);
    write_struct_to_nvs("stocksStat", &currentStocks, sizeof(Stocks_Stat_t));
    xSemaphoreGive(changeDispStock_Sem);
    ble_Application_Command_Respond_Success(finnhubStatsAppRoute, cmdNumber, pdPASS);
}

void add_RemoveStockSymbol(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String stockSymbol = dCommand["stkList"];
    // if(actionCmd) addStockSymbol(stockSymbolsFilePath, String(stockSymbol));
    // else removeStockSymbol(stockSymbolsFilePath, String(stockSymbol));

    stockSymbol.replace("[", "");
    stockSymbol.replace("]", "");

    printf("The Selected stocks are: %s\n", stockSymbol.c_str());

    File file = LittleFS.open("/stocks/dStocks.csv", "w");
        if (file) {
            file.print(stockSymbol);
            file.close();
            printf("Default stock symbols written to file.\n");

            // Read the symbols again after writing defaults
            readStockSymbols(stockSymbolsFilePath, stockSymbols, stockCount, MAX_STOCKS);
            printf("After writing defaults, found %d stock symbols:\n", stockCount);
            for (int i = 0; i < stockCount; i++) {
                printf("%s\n",stockSymbols[i].c_str());
            }
        } else {
            printf("Failed to write default symbols to file.\n");
            readStockSymbols(stockSymbolsFilePath, stockSymbols, stockCount, MAX_STOCKS);
        }

    
    // xSemaphoreGive(changeDispStock_Sem);
    ble_Application_Command_Respond_Success(finnhubStatsAppRoute, cmdNumber, pdPASS);
}

void setStockChangeInterval(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    uint8_t setCycle = dCommand["cycleStocks"];
    int16_t dInterval = dCommand["dInterval"];

    if(setCycle == pdTRUE && xTimerIsTimerActive(stockChangeTimer_H) == pdFALSE){
        currentStocks.stockChangeInterval = dInterval;
        xTimerStart(stockChangeTimer_H, 0);
    } else if (setCycle == pdFALSE && xTimerIsTimerActive(stockChangeTimer_H) == pdTRUE){
        currentStocks.stockChangeInterval = dInterval;
        xTimerStop(stockChangeTimer_H, 0);
    } else if (setCycle == pdTRUE && xTimerIsTimerActive(stockChangeTimer_H) == pdTRUE && dInterval != currentStocks.stockChangeInterval){
        currentStocks.stockChangeInterval = dInterval;
        xTimerStop(stockChangeTimer_H, 0);
        xTimerChangePeriod(stockChangeTimer_H, pdMS_TO_TICKS(dInterval * 1000), 0);
        xTimerStart(stockChangeTimer_H, 0);
    }
    write_struct_to_nvs("stocksStat", &currentStocks, sizeof(Stocks_Stat_t));
    ble_Application_Command_Respond_Success(finnhubStatsAppRoute, cmdNumber, pdPASS);
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

// void loadSavedStocks(DynamicJsonDocument& dCommand){
//     uint8_t cmdNumber = dCommand["app_command"];
//     String savedSymbols = "{\"pxp_command\":";
//     savedSymbols += String(cmdNumber) + ",\"savedSymbols\":" + convertArrayToJson(stockSymbols, stockCount) + "}";
//     bleApplicationComSend(savedSymbols);
// }

void saveAPI_key(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String userAPI_Key = dCommand["api_key"];
    strcpy(currentStocks.apiToken, userAPI_Key.c_str());
    write_struct_to_nvs("stocksStat", &currentStocks, sizeof(Stocks_Stat_t));
    ble_Application_Command_Respond_Success(finnhubStatsAppRoute, cmdNumber, pdPASS);
}
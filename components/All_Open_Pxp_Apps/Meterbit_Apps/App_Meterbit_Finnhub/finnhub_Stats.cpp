#include "mtb_engine.h"
#include <HTTPClient.h>
#include "mtb_github.h"
#include "mtb_text_scroll.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include "finnhub_Stats.h"

static const char TAG[] = "FINNHUB_STATS";

#define MAX_STOCKS 100

// Default Stock
Stocks_Stat_t currentStocks = {
    "AAPL",
    "USD",
    "/stocks/stocksIcon_1.png",
    "get free key @ finnhub.io/register",
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

void showParticularStock(JsonDocument&);
void add_RemoveStockSymbol(JsonDocument&);
void setStockChangeInterval(JsonDocument&);
void saveAPI_key(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *finnhub_Stats_App = new Mtb_Applications_StatusBar(finhubStats_App_Task, &finhubStats_Task_H, "Finnhub Stats", 10240);

void finhubStats_App_Task(void* dApplication){
    Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
    thisApp->mtb_App_EncoderFn_ptr = mtb_Brightness_Control;
    thisApp->mtb_App_ButtonFn_ptr = buttonChangeDisplayStock;
    mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(showParticularStock, add_RemoveStockSymbol, setStockChangeInterval, saveAPI_key);
    mtb_App_Init(thisApp, mtb_Status_Bar_Clock_Sv);
    //************************************************************************************ */
    mtb_Read_Nvs_Struct("stocksStat", &currentStocks, sizeof(Stocks_Stat_t));
    time_t present = 0;
    if(changeDispStock_Sem == NULL) changeDispStock_Sem = xSemaphoreCreateBinary();
    if(stockChangeTimer_H == NULL) stockChangeTimer_H = xTimerCreate("stockChange Timer", pdMS_TO_TICKS(currentStocks.stockChangeInterval > 0 ? (currentStocks.stockChangeInterval * 1000) : (30 * 1000)), true, NULL, stockChange_TimerCallback);
    StaticJsonDocument<3072> doc;

    Mtb_FixedText_t stockID_tag(40, 13, Terminal6x8, CYAN);
    Mtb_FixedText_t current_price_tag(40, 23, Terminal6x8, YELLOW);
    Mtb_FixedText_t priceChangePercent_tag(40, 33, Terminal6x8, YELLOW_GREEN);    
    Mtb_FixedText_t cPrice_diff_tag(40, 43, Terminal6x8, GREEN);
    Mtb_ScrollText_t moreStockData (0, 55, 128, WHITE, 20, 1, Terminal6x8);

    stockID_tag.mtb_Write_String("STK:");
    current_price_tag.mtb_Write_String("PRC:");
    cPrice_diff_tag.mtb_Write_String("DPR:");
    priceChangePercent_tag.mtb_Write_String("G/L:");

    Mtb_FixedText_t stockID_txt(67, 13, Terminal6x8, CYAN);
    Mtb_FixedText_t current_price_txt(67, 23, Terminal6x8, YELLOW);
    Mtb_FixedText_t priceChangePercent_txt(67, 33, Terminal6x8, YELLOW_GREEN);    
    Mtb_FixedText_t cPrice_diff_txt(67, 43, Terminal6x8, GREEN);
//##############################################################################################################
    
    // Read stock symbols from the CSV file
    readStockSymbols(stockSymbolsFilePath, stockSymbols, stockCount, MAX_STOCKS);
    
    //ESP_LOGI(TAG, "Found %d stock symbols:\n", stockCount);
    // for (int i = 0; i < stockCount; i++) {
    //     ESP_LOGI(TAG, "%s\n",stockSymbols[i].c_str());
    // }

    // If no symbols are found, write default symbols to the file
    if (stockCount == 0) {
        File file = LittleFS.open(stockSymbolsFilePath, "w");
        if (file) {
            String defaultSymbols = "AAPL,NVDA,AMZN,TSLA,MSFT,GOOG";
            file.print(defaultSymbols);
            file.close();
            ESP_LOGI(TAG, "Default stock symbols written to file.\n");

            // Read the symbols again after writing defaults
            readStockSymbols(stockSymbolsFilePath, stockSymbols, stockCount, MAX_STOCKS);
            ESP_LOGI(TAG, "After writing defaults, found %d stock symbols:\n", stockCount);
            for (int i = 0; i < stockCount; i++) {
                ESP_LOGI(TAG, "%s\n",stockSymbols[i].c_str());
            }
        } else {
            ESP_LOGI(TAG, "Failed to write default symbols to file.\n");
        }
    } 
    
    if(currentStocks.stockChangeInterval > 1) xTimerStart(stockChangeTimer_H, 0);
//##############################################################################################################
//##############################################################################################################
char apiUrl[1000]; // Adjust size as needed
static HTTPClient http;

while (MTB_APP_IS_ACTIVE == pdTRUE){
    snprintf(apiUrl, sizeof(apiUrl), "https://finnhub.io/api/v1/quote?symbol=%s&token=%s", currentStocks.stockID.c_str(), currentStocks.apiToken);
    //ESP_LOGI(TAG, "OUR FINAL URL IS: %s \n", apiUrl.c_str());
    //******************************************************************************************************* */
    mtb_Download_Github_Strg_File("stocks_Icons/_" + currentStocks.stockID +".png", String(currentStocks.stockFilePath));

    while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);
        
        if (http.connected()) { http.end(); } // Cleanup before starting a new request

    //************************************************************************************** */
    while (MTB_APP_IS_ACTIVE == pdTRUE){
        int16_t stockDataRequestTim = 500;
        http.begin(apiUrl); // Edit your key here
        // http.addHeader("x-rapidapi-key", "783152efb6mshac636e27a8f24bep10fb3ajsn5ae13bc4f722");
        int httpCode = http.GET();
        if (httpCode > 0){
            String payload = http.getString();
            //ESP_LOGI(TAG, "\n\n Payload: %s \n\n", payload.c_str());

            DeserializationError error = deserializeJson(doc, payload);
            if (error) {
                ESP_LOGI(TAG, "deserializeJson() failed: %s\n", error.c_str());
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

            mtb_Draw_Local_Png({"/stocks/stocksIcon_1.png", 5, 18, 2});

            stockID_txt.mtb_Write_String(currentStocks.stockID);
            current_price_txt.mtb_Write_String(String(current_price));
            cPrice_diff_tag.mtb_Write_Colored_String("DPR:", price_Diff < 0 ? RED : GREEN);
            cPrice_diff_txt.mtb_Write_Colored_String(String(price_Diff), price_Diff < 0 ? RED : GREEN);
            priceChangePercent_tag.mtb_Write_Colored_String("G/L:", price_change_percentage_24h < 0 ? ORANGE : YELLOW_GREEN);
            priceChangePercent_txt.mtb_Write_Colored_String(String(price_change_percentage_24h) + "%", price_change_percentage_24h < 0 ? ORANGE : YELLOW_GREEN);

            if(price_Diff < 0) mtb_Draw_Local_Png({"/gain_lose/lose.png", 104, 20, 1});
            else mtb_Draw_Local_Png({"/gain_lose/gain.png", 104, 20, 1});

            moreStockData.mtb_Scroll_This_Text("CURR: " + currentStocks.currency, CYAN);
            moreStockData.mtb_Scroll_This_Text("HIGH: " + String(high24), GREEN);
            moreStockData.mtb_Scroll_This_Text("LOW: " + String(low24), YELLOW);
            moreStockData.mtb_Scroll_This_Text("OPEN: " + String(openPrice24), GREEN_CYAN);
            moreStockData.mtb_Scroll_This_Text("PCPR: " + String(previouClosePrice24), ORANGE);
            moreStockData.mtb_Scroll_This_Text("TIME: " + mtb_Unix_Time_To_Readable(present), YELLOW);

    } else {
        ESP_LOGI(TAG, "Error on HTTP request: %d \n", httpCode);
    }
        // Close the connection
        http.end();

        while(stockDataRequestTim-->0  && MTB_APP_IS_ACTIVE && xSemaphoreTake(changeDispStock_Sem, 0) != pdTRUE) delay(10);
        if(stockDataRequestTim > 0) break;
    }
        moreStockData.mtb_Scroll_Active(STOP_SCROLL);
}
  mtb_End_This_App(thisApp);
}
//##############################################################################################################

void readStockSymbols(const char* filename, String stockSymbols[], int& count, const int maxSymbols){
    
    if (mtb_Prepare_Flash_File_Path(filename)){
        ESP_LOGI(TAG, "File path prepared successfully: %s\n", filename);
    }
    else {
        ESP_LOGI(TAG, "Failed to prepare file path.\n");
        count = 0;
        return;
    }
    // Check if the file exists
    if (!LittleFS.exists(filename)) {
        // Create a new file if it doesn't exist
        File file = LittleFS.open(filename, "w");
        if (!file) {
            ESP_LOGI(TAG, "Failed to create file.\n");
            count = 0;
            return;
        }
        file.close();
        ESP_LOGI(TAG, "File created successfully.\n");
        count = 0;
        return;
    }
    
    // Open the file for reading
    File file = LittleFS.open(filename, "r");
    if (!file) {
        ESP_LOGI(TAG, "Failed to open file.\n");
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
                mtb_Write_Nvs_Struct("stocksStat", &currentStocks, sizeof(Stocks_Stat_t));
                xSemaphoreGive(changeDispStock_Sem);
                break;

            case BUTTON_PRESSED_LONG:
            if (xTimerIsTimerActive(stockChangeTimer_H) == pdTRUE) xTimerStop(stockChangeTimer_H, 0);
            else xTimerStart(stockChangeTimer_H, 0);
            break;

            case BUTTON_CLICKED:
            //ESP_LOGI(TAG, "Button Clicked: %d Times\n",button_Data.count);
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
    mtb_Write_Nvs_Struct("stocksStat", &currentStocks, sizeof(Stocks_Stat_t));
    xSemaphoreGive(changeDispStock_Sem);
}

void showParticularStock(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    const char *stockSymbol = dCommand["stkSymbol"];
    currentStocks.stockID = String(stockSymbol);
    mtb_Write_Nvs_Struct("stocksStat", &currentStocks, sizeof(Stocks_Stat_t));
    xSemaphoreGive(changeDispStock_Sem);
    mtb_Ble_App_Cmd_Respond_Success(finnhubStatsAppRoute, cmdNumber, pdPASS);
}

void add_RemoveStockSymbol(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String stockSymbol = dCommand["stkList"];
    // if(actionCmd) addStockSymbol(stockSymbolsFilePath, String(stockSymbol));
    // else removeStockSymbol(stockSymbolsFilePath, String(stockSymbol));

    stockSymbol.replace("[", "");
    stockSymbol.replace("]", "");

    ESP_LOGI(TAG, "The Selected stocks are: %s\n", stockSymbol.c_str());

    File file = LittleFS.open("/stocks/dStocks.csv", "w");
        if (file) {
            file.print(stockSymbol);
            file.close();
            ESP_LOGI(TAG, "Default stock symbols written to file.\n");

            // Read the symbols again after writing defaults
            readStockSymbols(stockSymbolsFilePath, stockSymbols, stockCount, MAX_STOCKS);
            ESP_LOGI(TAG, "After writing defaults, found %d stock symbols:\n", stockCount);
            for (int i = 0; i < stockCount; i++) {
                ESP_LOGI(TAG, "%s\n",stockSymbols[i].c_str());
            }
        } else {
            ESP_LOGI(TAG, "Failed to write default symbols to file.\n");
            readStockSymbols(stockSymbolsFilePath, stockSymbols, stockCount, MAX_STOCKS);
        }

    
    // xSemaphoreGive(changeDispStock_Sem);
    mtb_Ble_App_Cmd_Respond_Success(finnhubStatsAppRoute, cmdNumber, pdPASS);
}

void setStockChangeInterval(JsonDocument& dCommand){
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
    mtb_Write_Nvs_Struct("stocksStat", &currentStocks, sizeof(Stocks_Stat_t));
    mtb_Ble_App_Cmd_Respond_Success(finnhubStatsAppRoute, cmdNumber, pdPASS);
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

// void loadSavedStocks(JsonDocument& dCommand){
//     uint8_t cmdNumber = dCommand["app_command"];
//     String savedSymbols = "{\"pxp_command\":";
//     savedSymbols += String(cmdNumber) + ",\"savedSymbols\":" + convertArrayToJson(stockSymbols, stockCount) + "}";
//     bleApplicationComSend(savedSymbols);
// }

void saveAPI_key(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String userAPI_Key = dCommand["api_key"];
    strcpy(currentStocks.apiToken, userAPI_Key.c_str());
    mtb_Write_Nvs_Struct("stocksStat", &currentStocks, sizeof(Stocks_Stat_t));
    mtb_Ble_App_Cmd_Respond_Success(finnhubStatsAppRoute, cmdNumber, pdPASS);
}
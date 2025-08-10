#include "mtb_engine.h"
#include <HTTPClient.h>
#include "mtb_github.h"
#include "mtb_text_scroll.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include "crypto_Stats.h"


static const char TAG[] = "CRYPTO_STATS";

#define MAX_COINS 100

// Default CryptoCurrency
Crypto_Stat_t currentCryptoCurrency = {
    "bitcoin",
    "btc",
    "usd",
    "/crypto/cryptIcon_1.png",
    "Hello_API_Token",
    30
};

EXT_RAM_BSS_ATTR SemaphoreHandle_t changeDispCrypto_Sem = NULL;
EXT_RAM_BSS_ATTR TimerHandle_t cryptoChangeTimer_H = NULL;
int8_t cryptoIterate = 0;

String cryptoSymbols[MAX_COINS];
String cryptoIDs[MAX_COINS];
int cryptoCount = 0;
static const char cryptoSymbolsFilePath[] = "/crypto/dCrypto.csv";
static const char cryptoIDsFilePath[] = "/crypto/dCryptoID.csv";

EXT_RAM_BSS_ATTR TaskHandle_t cryptoStats_Task_H = NULL;
void cryptoStats_App_Task(void *);
void readCryptoSymbols(const char *filename, String coinSymbols[], int &count, const int maxSymbols);
void buttonChangeDisplayCrypto(button_event_t button_Data);
void crytoChange_TimerCallback(TimerHandle_t);

void showParticularCrypto(JsonDocument&);
void add_RemoveCryptoSymbol(JsonDocument&);
void setCryptoChangeInterval(JsonDocument&);
void setCrytoAPI_key(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *crypto_Stats_App = new Mtb_Applications_StatusBar(cryptoStats_App_Task, &cryptoStats_Task_H, "Crypto Data", 10240);

void cryptoStats_App_Task(void* dApplication){
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_EncoderFn_ptr = mtb_Brightness_Control;
  thisApp->mtb_App_ButtonFn_ptr = buttonChangeDisplayCrypto;
  mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(showParticularCrypto, add_RemoveCryptoSymbol, setCryptoChangeInterval, setCrytoAPI_key);
  mtb_App_Init(thisApp, mtb_Status_Bar_Clock_Sv);
  //************************************************************************************ */
  mtb_Read_Nvs_Struct("cryptoCur", &currentCryptoCurrency, sizeof(Crypto_Stat_t));
  if(changeDispCrypto_Sem == NULL) changeDispCrypto_Sem = xSemaphoreCreateBinary();
  if(cryptoChangeTimer_H == NULL) cryptoChangeTimer_H = xTimerCreate("cryptoIntvTim", pdMS_TO_TICKS(currentCryptoCurrency.cryptoChangeInterval>0 ? (currentCryptoCurrency.cryptoChangeInterval * 1000) : (30 * 1000)), true, NULL, crytoChange_TimerCallback);
  JsonDocument doc;
  DeserializationError error;

  Mtb_FixedText_t coinSymbol_tag(38, 13, Terminal6x8, YELLOW);
  Mtb_FixedText_t current_price_tag(38, 23, Terminal6x8, CYAN);
  Mtb_FixedText_t price_change_percentage_24h_tag(38, 33, Terminal6x8, GREEN);
  Mtb_FixedText_t vwap24Hr_tag(38, 43, Terminal6x8, MAGENTA);
  Mtb_ScrollText_t moreCryptoData (0, 55, 128, WHITE, 20, 1, Terminal6x8);

  coinSymbol_tag.mtb_Write_String("SYM:");
  current_price_tag.mtb_Write_String("PRC:");
  price_change_percentage_24h_tag.mtb_Write_String("D24:");
  vwap24Hr_tag.mtb_Write_String("VWA:");

  Mtb_FixedText_t coinSymbol_txt(63, 13, Terminal6x8, YELLOW);
  Mtb_FixedText_t current_price_txt(63, 23, Terminal6x8, CYAN);
  Mtb_FixedText_t price_change_percentage_24h_txt(63, 33, Terminal6x8, GREEN);
  Mtb_FixedText_t vwap24Hr_txt(63, 43, Terminal6x8, MAGENTA);
  //Mtb_FixedText_t market_cap_rank_txt(71, 53, Terminal6x8, LEMON);
  //*********************************************************************************************************

  // Read cryto symbols and IDs from the CSV files
  readCryptoSymbols(cryptoSymbolsFilePath, cryptoSymbols, cryptoCount, MAX_COINS);
  readCryptoSymbols(cryptoIDsFilePath, cryptoIDs, cryptoCount, MAX_COINS);

  ESP_LOGI(TAG, "Found %d coins symbols:\n", cryptoCount);
  for (int i = 0; i < cryptoCount; i++) {
      ESP_LOGI(TAG, "%s\n", cryptoSymbols[i].c_str());
  }

ESP_LOGI(TAG, "Found %d coins IDs:\n", cryptoCount);
  for (int i = 0; i < cryptoCount; i++) {
      ESP_LOGI(TAG, "%s\n", cryptoIDs[i].c_str());
  }

    // If no symbols are found, write default symbols to the file
    if (cryptoCount == 0) {
        File file = LittleFS.open(cryptoSymbolsFilePath, "w");
        if (file) {
            String defaultSymbols = "BTC,ETH,DOGE";
            file.print(defaultSymbols);
            file.close();
            ESP_LOGI(TAG, "Default cryptoCoin symbols written to file.\n");

            // Read the symbols again after writing defaults
            readCryptoSymbols(cryptoSymbolsFilePath, cryptoSymbols, cryptoCount, MAX_COINS);
            ESP_LOGI(TAG, "After writing defaults, found %d cryptoCoin symbols:\n", cryptoCount);
            for (int i = 0; i < cryptoCount; i++) {
                ESP_LOGI(TAG, "%s\n",cryptoSymbols[i].c_str());
            }
        } else {
            ESP_LOGI(TAG, "Failed to write default symbols to file.\n");
        }

        File file2 = LittleFS.open(cryptoIDsFilePath, "w");
        if (file2) {
            String defaultIDs = "bitcoin, ethereum, dogecoin";
            file2.print(defaultIDs);
            file2.close();
            ESP_LOGI(TAG, "Default cryptoCoin IDs written to file.\n");

            // Read the symbols again after writing defaults
            readCryptoSymbols(cryptoIDsFilePath, cryptoIDs, cryptoCount, MAX_COINS);
            ESP_LOGI(TAG, "After writing defaults, found %d cryptoCoin symbols:\n", cryptoCount);
            for (int i = 0; i < cryptoCount; i++) {
                ESP_LOGI(TAG, "%s\n",cryptoIDs[i].c_str());
            }
        } else {
            ESP_LOGI(TAG, "Failed to write default symbols to file.\n");
        }
    } 

    if(currentCryptoCurrency.cryptoChangeInterval > 1) xTimerStart(cryptoChangeTimer_H, 0);
//##############################################################################################################
//##############################################################################################################

while (MTB_APP_IS_ACTIVE == pdTRUE) {
    // ==================== LOAD NEW COIN FOR DISPLAY ====================
    String apiUrl = "https://rest.coincap.io/v3/assets/";
    apiUrl += currentCryptoCurrency.coinID;
    ESP_LOGI(TAG, "OUR FINAL URL IS: %s \n", apiUrl.c_str());

    currentCryptoCurrency.coinSymbol.toLowerCase();
    mtb_Download_Github_Strg_File("cryp_Icons/" + currentCryptoCurrency.coinSymbol + ".png", "/crypto/cryptIcon_1.png");

    while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);

    // ==================== FETCH AND UPDATE EVERY INTERVAL ====================
    while (MTB_APP_IS_ACTIVE == pdTRUE) {
        int16_t cryptoDataRequestTim = 20000; // 20 seconds
        HTTPClient http;
        http.begin(apiUrl);
        http.addHeader("Authorization", "Bearer " + String(currentCryptoCurrency.apiToken));

        int httpResponseCode = http.GET();

        if (httpResponseCode == 200) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error && doc.containsKey("data")) {
                JsonObject data = doc["data"];

                String coinSymbol = data["symbol"];
                String coinPrice = data["priceUsd"];
                String price_change_percentage_24h = data["changePercent24Hr"];
                String vwap24Hr = data["vwap24Hr"];

                String coinName = data["name"];
                String coinRank = data["rank"];
                String market_cap = data["marketCapUsd"];
                String circulating_supply = data["supply"];

                mtb_Draw_Local_Png({"/crypto/cryptIcon_1.png", 3, 16});

                double coinPrice_Double = coinPrice.toDouble();
                current_price_txt.mtb_Write_String(String(coinPrice_Double, coinPrice_Double < 100 ? 4 : 2));
                coinSymbol_txt.mtb_Write_String(coinSymbol);

                price_change_percentage_24h_tag.mtb_Write_Colored_String("D24:", price_change_percentage_24h.toDouble() < 0 ? RED : GREEN);
                double priceChngPercent_Double = price_change_percentage_24h.toDouble();
                price_change_percentage_24h_txt.mtb_Write_Colored_String(String(priceChngPercent_Double, 2) + "%", priceChngPercent_Double < 0 ? RED : GREEN);

                double vwap24Hours = vwap24Hr.toDouble();
                vwap24Hr_txt.mtb_Write_String(String(vwap24Hours, vwap24Hours < 100 ? 4 : 2));

                if(price_change_percentage_24h.toDouble() < 0) mtb_Draw_Local_Png({"/gain_lose/lose.png", 104, 20, 1});
                else mtb_Draw_Local_Png({"/gain_lose/gain.png", 104, 20, 1});

                moreCryptoData.mtb_Scroll_This_Text("COIN: " + coinName, YELLOW);
                moreCryptoData.mtb_Scroll_This_Text("RANK: " + coinRank, FUCHSIA);
                moreCryptoData.mtb_Scroll_This_Text("CURR: USD", CYAN);
                double mkt_Cap_Double = market_cap.toDouble();
                moreCryptoData.mtb_Scroll_This_Text("MKT CAP: " + formatLargeNumber(mkt_Cap_Double) + " USD", GREEN);
                double cir_Supply = circulating_supply.toDouble();
                moreCryptoData.mtb_Scroll_This_Text("CIR SUP: " + formatLargeNumber(cir_Supply) + " " + coinSymbol, ORANGE);
            } else {
                ESP_LOGI(TAG, "JSON parse error or no data key.\n");
            }
        } else {
            ESP_LOGI(TAG, "Error on HTTP request: %d \n", httpResponseCode);
        }

        http.end();

        while (cryptoDataRequestTim-- > 0 && MTB_APP_IS_ACTIVE && xSemaphoreTake(changeDispCrypto_Sem, 0) != pdTRUE) delay(1);
        if (cryptoDataRequestTim > 0) break;
    }

    moreCryptoData.mtb_Scroll_Active(STOP_SCROLL);
}

  mtb_End_This_App(thisApp);
}

void readCryptoSymbols(const char* filename, String coinSymbols[], int& count, const int maxSymbols){
    
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
    
    // Parse the CSV data to extract cryptoCoin symbols
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
                coinSymbols[count++] = symbol;
            }
            break;
        } else {
            String symbol = content.substring(start, commaIndex);
            symbol.trim();
            if (symbol.length() > 0) {
                coinSymbols[count++] = symbol;
            }
            start = commaIndex + 1;
        }
    }
}

void buttonChangeDisplayCrypto(button_event_t button_Data){
            switch (button_Data.type){
            case BUTTON_RELEASED:
            break;

            case BUTTON_PRESSED:
                if ((++cryptoIterate) < cryptoCount) currentCryptoCurrency.coinID = cryptoSymbols[cryptoIterate];
                else currentCryptoCurrency.coinID = cryptoSymbols[cryptoIterate = 0];
                mtb_Write_Nvs_Struct("cryptoCur", &currentCryptoCurrency, sizeof(Crypto_Stat_t));
                xSemaphoreGive(changeDispCrypto_Sem);
                break;

            case BUTTON_PRESSED_LONG:
            if (xTimerIsTimerActive(cryptoChangeTimer_H) == pdTRUE) xTimerStop(cryptoChangeTimer_H, 0);
            else xTimerStart(cryptoChangeTimer_H, 0);
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

void crytoChange_TimerCallback(TimerHandle_t coinPrompt){
    if ((++cryptoIterate) < cryptoCount){ 
        currentCryptoCurrency.coinSymbol = cryptoSymbols[cryptoIterate];
        currentCryptoCurrency.coinID = cryptoIDs[cryptoIterate];
        }
    else {
        currentCryptoCurrency.coinSymbol = cryptoSymbols[cryptoIterate = 0];
        currentCryptoCurrency.coinID = cryptoIDs[cryptoIterate = 0];
        }
    mtb_Write_Nvs_Struct("cryptoCur", &currentCryptoCurrency, sizeof(Crypto_Stat_t));
    xSemaphoreGive(changeDispCrypto_Sem);
}

void showParticularCrypto(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String coinSymbol = dCommand["coinSymbol"];
    currentCryptoCurrency.coinSymbol = coinSymbol;
    String coinIDs = dCommand["coinID"];
    currentCryptoCurrency.coinID = coinIDs;
    mtb_Write_Nvs_Struct("cryptoCur", &currentCryptoCurrency, sizeof(Crypto_Stat_t));
    xSemaphoreGive(changeDispCrypto_Sem);
    mtb_Ble_App_Cmd_Respond_Success(cryptoStatsAppRoute, cmdNumber, pdPASS);
}

void add_RemoveCryptoSymbol(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String coinSymbols = dCommand["coinSymbols"];
    String coinIDs = dCommand["coinIDs"];

    coinSymbols.replace("[", "");
    coinSymbols.replace("]", "");

    coinIDs.replace("[", "");
    coinIDs.replace("]", "");

    // ESP_LOGI(TAG, "The Selected crypto Symbols are: %s\n", coinSymbols.c_str());
    // ESP_LOGI(TAG, "The Selected crypto IDs are: %s\n", coinIDs.c_str());

    File file = LittleFS.open(cryptoSymbolsFilePath, "w");
        if (file) {
            file.print(coinSymbols);
            file.close();
            ESP_LOGI(TAG, "Default crypto symbols written to file.\n");

            // Read the symbols again after writing defaults
            readCryptoSymbols(cryptoSymbolsFilePath, cryptoSymbols, cryptoCount, MAX_COINS);
            ESP_LOGI(TAG, "After writing defaults, found %d cryptoCoin symbols:\n", cryptoCount);
            for (int i = 0; i < cryptoCount; i++) {
                ESP_LOGI(TAG, "%s\n", cryptoSymbols[i].c_str());
            }
        } else {
            ESP_LOGI(TAG, "Failed to write default symbols to file.\n");
            readCryptoSymbols(cryptoSymbolsFilePath, cryptoSymbols, cryptoCount, MAX_COINS);
        }

    File file2 = LittleFS.open(cryptoIDsFilePath, "w");
        if (file2) {
            file2.print(coinIDs);
            file2.close();
            ESP_LOGI(TAG, "Default crypto IDs written to file.\n");

            // Read the symbols again after writing defaults
            readCryptoSymbols(cryptoIDsFilePath, cryptoIDs, cryptoCount, MAX_COINS);
            ESP_LOGI(TAG, "After writing defaults, found %d cryptoCoin IDs:\n", cryptoCount);
            for (int i = 0; i < cryptoCount; i++) {
                ESP_LOGI(TAG, "%s\n", cryptoIDs[i].c_str());
            }
        } else {
            ESP_LOGI(TAG, "Failed to write default coinIDs to file.\n");
            readCryptoSymbols(cryptoIDsFilePath, cryptoIDs, cryptoCount, MAX_COINS);
        }
    
    //xSemaphoreGive(changeDispCrypto_Sem);
    mtb_Ble_App_Cmd_Respond_Success(cryptoStatsAppRoute, cmdNumber, pdPASS);
}

void setCryptoChangeInterval(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    uint8_t setCycle = dCommand["cycleCoins"];
    int16_t dInterval = dCommand["dInterval"];

    if(setCycle == pdTRUE && xTimerIsTimerActive(cryptoChangeTimer_H) == pdFALSE){
        currentCryptoCurrency.cryptoChangeInterval = dInterval;
        xTimerStart(cryptoChangeTimer_H, 0);
    } else if (setCycle == pdFALSE && xTimerIsTimerActive(cryptoChangeTimer_H) == pdTRUE){
        currentCryptoCurrency.cryptoChangeInterval = dInterval;
        xTimerStop(cryptoChangeTimer_H, 0);
    } else if (setCycle == pdTRUE && xTimerIsTimerActive(cryptoChangeTimer_H) == pdTRUE && dInterval != currentCryptoCurrency.cryptoChangeInterval){
        currentCryptoCurrency.cryptoChangeInterval = dInterval;
        xTimerStop(cryptoChangeTimer_H, 0);
        xTimerChangePeriod(cryptoChangeTimer_H, pdMS_TO_TICKS(dInterval * 1000), 0);
        xTimerStart(cryptoChangeTimer_H, 0);
    }
    mtb_Write_Nvs_Struct("cryptoCur", &currentCryptoCurrency, sizeof(Crypto_Stat_t));
    mtb_Ble_App_Cmd_Respond_Success(cryptoStatsAppRoute, cmdNumber, pdPASS);
}

void setCrytoAPI_key(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    strcpy(currentCryptoCurrency.apiToken, (const char*) dCommand["api_key"]);
    mtb_Write_Nvs_Struct("cryptoCur", &currentCryptoCurrency, sizeof(Crypto_Stat_t));
    mtb_Ble_App_Cmd_Respond_Success(cryptoStatsAppRoute, cmdNumber, pdPASS);
}
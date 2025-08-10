#include "mtb_engine.h"
#include <HTTPClient.h>
#include "mtb_github.h"
#include "mtb_text_scroll.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include "crypto_Stats.h"

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

void showParticularCrypto(DynamicJsonDocument&);
void add_RemoveCryptoSymbol(DynamicJsonDocument&);
void setCryptoChangeInterval(DynamicJsonDocument&);
void setCrytoAPI_key(DynamicJsonDocument&);

EXT_RAM_BSS_ATTR Applications_StatusBar *crypto_Stats_App = new Applications_StatusBar(cryptoStats_App_Task, &cryptoStats_Task_H, "Crypto Data", 10240);

void cryptoStats_App_Task(void* dApplication){
  Applications *thisApp = (Applications *)dApplication;
  thisApp->app_EncoderFn_ptr = brightnessControl;
  thisApp->app_ButtonFn_ptr = buttonChangeDisplayCrypto;
  ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(showParticularCrypto, add_RemoveCryptoSymbol, setCryptoChangeInterval, setCrytoAPI_key);
  appsInitialization(thisApp, statusBarClock_Sv);
  //************************************************************************************ */
  read_struct_from_nvs("cryptoCur", &currentCryptoCurrency, sizeof(Crypto_Stat_t));
  if(changeDispCrypto_Sem == NULL) changeDispCrypto_Sem = xSemaphoreCreateBinary();
  if(cryptoChangeTimer_H == NULL) cryptoChangeTimer_H = xTimerCreate("cryptoIntvTim", pdMS_TO_TICKS(currentCryptoCurrency.cryptoChangeInterval>0 ? (currentCryptoCurrency.cryptoChangeInterval * 1000) : (30 * 1000)), true, NULL, crytoChange_TimerCallback);
  DynamicJsonDocument doc(3072);
  DeserializationError error;

  FixedText_t coinSymbol_tag(38, 13, Terminal6x8, YELLOW);
  FixedText_t current_price_tag(38, 23, Terminal6x8, CYAN);
  FixedText_t price_change_percentage_24h_tag(38, 33, Terminal6x8, GREEN);
  FixedText_t vwap24Hr_tag(38, 43, Terminal6x8, MAGENTA);
  ScrollText_t moreCryptoData (0, 55, 128, WHITE, 20, 1, Terminal6x8);

  coinSymbol_tag.writeString("SYM:");
  current_price_tag.writeString("PRC:");
  price_change_percentage_24h_tag.writeString("D24:");
  vwap24Hr_tag.writeString("VWA:");

  FixedText_t coinSymbol_txt(63, 13, Terminal6x8, YELLOW);
  FixedText_t current_price_txt(63, 23, Terminal6x8, CYAN);
  FixedText_t price_change_percentage_24h_txt(63, 33, Terminal6x8, GREEN);
  FixedText_t vwap24Hr_txt(63, 43, Terminal6x8, MAGENTA);
  //FixedText_t market_cap_rank_txt(71, 53, Terminal6x8, LEMON);
  //*********************************************************************************************************

  // Read cryto symbols and IDs from the CSV files
  readCryptoSymbols(cryptoSymbolsFilePath, cryptoSymbols, cryptoCount, MAX_COINS);
  readCryptoSymbols(cryptoIDsFilePath, cryptoIDs, cryptoCount, MAX_COINS);

  printf("Found %d coins symbols:\n", cryptoCount);
  for (int i = 0; i < cryptoCount; i++) {
      printf("%s\n", cryptoSymbols[i].c_str());
  }

printf("Found %d coins IDs:\n", cryptoCount);
  for (int i = 0; i < cryptoCount; i++) {
      printf("%s\n", cryptoIDs[i].c_str());
  }

    // If no symbols are found, write default symbols to the file
    if (cryptoCount == 0) {
        File file = LittleFS.open(cryptoSymbolsFilePath, "w");
        if (file) {
            String defaultSymbols = "BTC,ETH,DOGE";
            file.print(defaultSymbols);
            file.close();
            printf("Default cryptoCoin symbols written to file.\n");

            // Read the symbols again after writing defaults
            readCryptoSymbols(cryptoSymbolsFilePath, cryptoSymbols, cryptoCount, MAX_COINS);
            printf("After writing defaults, found %d cryptoCoin symbols:\n", cryptoCount);
            for (int i = 0; i < cryptoCount; i++) {
                printf("%s\n",cryptoSymbols[i].c_str());
            }
        } else {
            printf("Failed to write default symbols to file.\n");
        }

        File file2 = LittleFS.open(cryptoIDsFilePath, "w");
        if (file2) {
            String defaultIDs = "bitcoin, ethereum, dogecoin";
            file2.print(defaultIDs);
            file2.close();
            printf("Default cryptoCoin IDs written to file.\n");

            // Read the symbols again after writing defaults
            readCryptoSymbols(cryptoIDsFilePath, cryptoIDs, cryptoCount, MAX_COINS);
            printf("After writing defaults, found %d cryptoCoin symbols:\n", cryptoCount);
            for (int i = 0; i < cryptoCount; i++) {
                printf("%s\n",cryptoIDs[i].c_str());
            }
        } else {
            printf("Failed to write default symbols to file.\n");
        }
    } 

    if(currentCryptoCurrency.cryptoChangeInterval > 1) xTimerStart(cryptoChangeTimer_H, 0);
//##############################################################################################################
//##############################################################################################################

while (THIS_APP_IS_ACTIVE == pdTRUE) {
    // ==================== LOAD NEW COIN FOR DISPLAY ====================
    String apiUrl = "https://rest.coincap.io/v3/assets/";
    apiUrl += currentCryptoCurrency.coinID;
    printf("OUR FINAL URL IS: %s \n", apiUrl.c_str());

    currentCryptoCurrency.coinSymbol.toLowerCase();
    downloadGithubStrgFile("cryp_Icons/" + currentCryptoCurrency.coinSymbol + ".png", "/crypto/cryptIcon_1.png");

    while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);

    // ==================== FETCH AND UPDATE EVERY INTERVAL ====================
    while (THIS_APP_IS_ACTIVE == pdTRUE) {
        int16_t cryptoDataRequestTim = 20000; // 20 seconds
        HTTPClient http;
        http.begin(apiUrl);
        http.addHeader("Authorization", "Bearer " + String(currentCryptoCurrency.apiToken));

        int httpResponseCode = http.GET();

        if (httpResponseCode == 200) {
            String payload = http.getString();
            DynamicJsonDocument doc(3000);
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

                drawLocalPNG({"/crypto/cryptIcon_1.png", 3, 16});

                double coinPrice_Double = coinPrice.toDouble();
                current_price_txt.writeString(String(coinPrice_Double, coinPrice_Double < 100 ? 4 : 2));
                coinSymbol_txt.writeString(coinSymbol);

                price_change_percentage_24h_tag.writeColoredString("D24:", price_change_percentage_24h.toDouble() < 0 ? RED : GREEN);
                double priceChngPercent_Double = price_change_percentage_24h.toDouble();
                price_change_percentage_24h_txt.writeColoredString(String(priceChngPercent_Double, 2) + "%", priceChngPercent_Double < 0 ? RED : GREEN);

                double vwap24Hours = vwap24Hr.toDouble();
                vwap24Hr_txt.writeString(String(vwap24Hours, vwap24Hours < 100 ? 4 : 2));

                if(price_change_percentage_24h.toDouble() < 0) drawLocalPNG({"/gain_lose/lose.png", 104, 20, 1});
                else drawLocalPNG({"/gain_lose/gain.png", 104, 20, 1});

                moreCryptoData.scroll_This_Text("COIN: " + coinName, YELLOW);
                moreCryptoData.scroll_This_Text("RANK: " + coinRank, FUCHSIA);
                moreCryptoData.scroll_This_Text("CURR: USD", CYAN);
                double mkt_Cap_Double = market_cap.toDouble();
                moreCryptoData.scroll_This_Text("MKT CAP: " + formatLargeNumber(mkt_Cap_Double) + " USD", GREEN);
                double cir_Supply = circulating_supply.toDouble();
                moreCryptoData.scroll_This_Text("CIR SUP: " + formatLargeNumber(cir_Supply) + " " + coinSymbol, ORANGE);
            } else {
                printf("JSON parse error or no data key.\n");
            }
        } else {
            printf("Error on HTTP request: %d \n", httpResponseCode);
        }

        http.end();

        while (cryptoDataRequestTim-- > 0 && THIS_APP_IS_ACTIVE && xSemaphoreTake(changeDispCrypto_Sem, 0) != pdTRUE) delay(1);
        if (cryptoDataRequestTim > 0) break;
    }

    moreCryptoData.scroll_Active(STOP_SCROLL);
}

  kill_This_App(thisApp);
}

void readCryptoSymbols(const char* filename, String coinSymbols[], int& count, const int maxSymbols){
    
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
                write_struct_to_nvs("cryptoCur", &currentCryptoCurrency, sizeof(Crypto_Stat_t));
                xSemaphoreGive(changeDispCrypto_Sem);
                break;

            case BUTTON_PRESSED_LONG:
            if (xTimerIsTimerActive(cryptoChangeTimer_H) == pdTRUE) xTimerStop(cryptoChangeTimer_H, 0);
            else xTimerStart(cryptoChangeTimer_H, 0);
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

void crytoChange_TimerCallback(TimerHandle_t coinPrompt){
    if ((++cryptoIterate) < cryptoCount){ 
        currentCryptoCurrency.coinSymbol = cryptoSymbols[cryptoIterate];
        currentCryptoCurrency.coinID = cryptoIDs[cryptoIterate];
        }
    else {
        currentCryptoCurrency.coinSymbol = cryptoSymbols[cryptoIterate = 0];
        currentCryptoCurrency.coinID = cryptoIDs[cryptoIterate = 0];
        }
    write_struct_to_nvs("cryptoCur", &currentCryptoCurrency, sizeof(Crypto_Stat_t));
    xSemaphoreGive(changeDispCrypto_Sem);
}

void showParticularCrypto(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String coinSymbol = dCommand["coinSymbol"];
    currentCryptoCurrency.coinSymbol = coinSymbol;
    String coinIDs = dCommand["coinID"];
    currentCryptoCurrency.coinID = coinIDs;
    write_struct_to_nvs("cryptoCur", &currentCryptoCurrency, sizeof(Crypto_Stat_t));
    xSemaphoreGive(changeDispCrypto_Sem);
    ble_Application_Command_Respond_Success(cryptoStatsAppRoute, cmdNumber, pdPASS);
}

void add_RemoveCryptoSymbol(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String coinSymbols = dCommand["coinSymbols"];
    String coinIDs = dCommand["coinIDs"];

    coinSymbols.replace("[", "");
    coinSymbols.replace("]", "");

    coinIDs.replace("[", "");
    coinIDs.replace("]", "");

    // printf("The Selected crypto Symbols are: %s\n", coinSymbols.c_str());
    // printf("The Selected crypto IDs are: %s\n", coinIDs.c_str());

    File file = LittleFS.open(cryptoSymbolsFilePath, "w");
        if (file) {
            file.print(coinSymbols);
            file.close();
            printf("Default crypto symbols written to file.\n");

            // Read the symbols again after writing defaults
            readCryptoSymbols(cryptoSymbolsFilePath, cryptoSymbols, cryptoCount, MAX_COINS);
            printf("After writing defaults, found %d cryptoCoin symbols:\n", cryptoCount);
            for (int i = 0; i < cryptoCount; i++) {
                printf("%s\n", cryptoSymbols[i].c_str());
            }
        } else {
            printf("Failed to write default symbols to file.\n");
            readCryptoSymbols(cryptoSymbolsFilePath, cryptoSymbols, cryptoCount, MAX_COINS);
        }

    File file2 = LittleFS.open(cryptoIDsFilePath, "w");
        if (file2) {
            file2.print(coinIDs);
            file2.close();
            printf("Default crypto IDs written to file.\n");

            // Read the symbols again after writing defaults
            readCryptoSymbols(cryptoIDsFilePath, cryptoIDs, cryptoCount, MAX_COINS);
            printf("After writing defaults, found %d cryptoCoin IDs:\n", cryptoCount);
            for (int i = 0; i < cryptoCount; i++) {
                printf("%s\n", cryptoIDs[i].c_str());
            }
        } else {
            printf("Failed to write default coinIDs to file.\n");
            readCryptoSymbols(cryptoIDsFilePath, cryptoIDs, cryptoCount, MAX_COINS);
        }
    
    //xSemaphoreGive(changeDispCrypto_Sem);
    ble_Application_Command_Respond_Success(cryptoStatsAppRoute, cmdNumber, pdPASS);
}

void setCryptoChangeInterval(DynamicJsonDocument& dCommand){
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
    write_struct_to_nvs("cryptoCur", &currentCryptoCurrency, sizeof(Crypto_Stat_t));
    ble_Application_Command_Respond_Success(cryptoStatsAppRoute, cmdNumber, pdPASS);
}

void setCrytoAPI_key(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    strcpy(currentCryptoCurrency.apiToken, (const char*) dCommand["api_key"]);
    write_struct_to_nvs("cryptoCur", &currentCryptoCurrency, sizeof(Crypto_Stat_t));
    ble_Application_Command_Respond_Success(cryptoStatsAppRoute, cmdNumber, pdPASS);
}
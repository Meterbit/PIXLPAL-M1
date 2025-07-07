#include "mtbApps.h"
#include <HTTPClient.h>
#include "FS.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "scrollMsgs.h"

// Global variables and handles
EXT_RAM_BSS_ATTR SemaphoreHandle_t googleNewsupdateSem = NULL;
EXT_RAM_BSS_ATTR TimerHandle_t googleNewsUpdateTimer_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t googleNews_Task_H = NULL;
void googleNews_App_Task(void *);

uint32_t googleNewsUpdateInterval = 60000; // Update every 60 seconds by default
String googleNewsAPIKey = "d09aa633cf012091937a24b110fe7fe6"; // Replace with your actual GNews API token
String googleNewsLanguage = "en";  // Default language for headlines

// Pre-defined colors for each headline line (16-bit colors)
uint16_t headlineColors[] = { WHITE, CYAN, GREEN, YELLOW, ORANGE, MAGENTA, BLUE, RED };

#define MAX_HEADLINES 8
// Global array to hold ScrollText_t objects for each headline line.
ScrollText_t* headlineScrolls[MAX_HEADLINES] = {0};

// Forward declarations of BLE command functions
void showLatestNews(DynamicJsonDocument& dCommand);
void setNewsUpdateInterval(DynamicJsonDocument& dCommand);
void setNewsAPIKey(DynamicJsonDocument& dCommand);
void setNewsLanguage(DynamicJsonDocument& dCommand);


String fetchNewsHeadlines();
void displayNewsHeadlines(String headlines);


// Forward declaration for button event handler
void buttonNewsTickerHandler(button_event_t button_Data);

// Timer callback to trigger news updates.
void googleNews_TimerCallback(TimerHandle_t timer) {
    xSemaphoreGive(googleNewsupdateSem);
}

EXT_RAM_BSS_ATTR Applications_StatusBar *googleNews_App = new Applications_StatusBar(googleNews_App_Task, &googleNews_Task_H, "Crypto Data", 10240);

// Main task for the News Ticker app.
void googleNews_App_Task(void * dApplication) {
    Applications *thisApp = (Applications *)dApplication;
    
    // Register BLE command functions.
    ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(showLatestNews, setNewsUpdateInterval, setNewsAPIKey, setNewsLanguage);
    
    // Set button and encoder handlers.
    thisApp->app_ButtonFn_ptr = buttonNewsTickerHandler;
    thisApp->app_EncoderFn_ptr = brightnessControl;
    
    // Initialize app services.
    appsInitialization(thisApp, statusBarClock_Sv);
    
    if (googleNewsupdateSem == NULL)
        googleNewsupdateSem = xSemaphoreCreateBinary();
    if (googleNewsUpdateTimer_H == NULL)
        googleNewsUpdateTimer_H = xTimerCreate("newsUpdateTimer", pdMS_TO_TICKS(googleNewsUpdateInterval), pdTRUE, NULL, googleNews_TimerCallback);
    
    if (googleNewsUpdateInterval > 0)
        xTimerStart(googleNewsUpdateTimer_H, 0);
    
    // Perform an initial news fetch and display.
    String headlines = fetchNewsHeadlines();
    displayNewsHeadlines(headlines);
    
    // Main loop: update headlines when the semaphore is given (by timer or button press).
    while (THIS_APP_IS_ACTIVE == pdTRUE) {
        if (xSemaphoreTake(googleNewsupdateSem, portMAX_DELAY) == pdTRUE) {
            headlines = fetchNewsHeadlines();
            displayNewsHeadlines(headlines);
        }
    }
    kill_This_App(thisApp);
}

// This function uses GNews API to fetch the latest headlines.
// It returns a string with each headline separated by a newline.
String fetchNewsHeadlines() {
    String headlines;
    HTTPClient http;
    String apiUrl = "https://gnews.io/api/v4/top-headlines?token=" + googleNewsAPIKey + "&lang=" + googleNewsLanguage;
    http.begin(apiUrl);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0){
        String payload = http.getString();
        DynamicJsonDocument doc(20480);
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            if (doc.containsKey("articles")) {
                JsonArray articles = doc["articles"].as<JsonArray>();
                for (JsonObject article : articles) {
                    const char* title = article["title"];
                    if (title != nullptr) {
                        headlines += String(title) + "\n";
                    }
                }
                if (headlines.endsWith("\n")) {
                    headlines = headlines.substring(0, headlines.length() - 1);
                }
            } else {
                headlines = "No articles found.";
            }
        } else {
            headlines = "Error parsing news data.";
        }
    } else {
        headlines = "Failed to fetch news.";
    }
    http.end();
    return headlines;
}

// Helper function to display headlines using ScrollText_t.
// Each headline is shown on its own line (y = line * 8) with a unique color.
void displayNewsHeadlines(String headlines) {
    // Clear the display before updating
    //dma_display->fillScreen(BLACK);
    
    int headlineIndex = 0;
    int start = 0;
    while (start < headlines.length() && headlineIndex < MAX_HEADLINES) {
        int newlineIndex = headlines.indexOf('\n', start);
        String lineText;
        if (newlineIndex == -1) {
            lineText = headlines.substring(start);
            start = headlines.length();
        } else {
            lineText = headlines.substring(start, newlineIndex);
            start = newlineIndex + 1;
        }
        // For each line, create a ScrollText_t object if it doesn't already exist.
        // Use x=0, full width of 128, and y position as (line index * 8).
        if (headlineScrolls[headlineIndex] == NULL) {
            headlineScrolls[headlineIndex] = new ScrollText_t(
                0, 
                headlineIndex * 8, 
                128, 
                headlineColors[headlineIndex % (sizeof(headlineColors) / sizeof(headlineColors[0]))], 
                20, 
                1, 
                Terminal6x8
            );
        }
        // Set the scrolling text with the corresponding color.
        headlineScrolls[headlineIndex]->scroll_This_Text(
            lineText, 
            headlineColors[headlineIndex % (sizeof(headlineColors) / sizeof(headlineColors[0]))]
        );
        headlineIndex++;
    }
    // Stop scrolling for any remaining ScrollText_t objects if fewer headlines are available.
    for (int i = headlineIndex; i < MAX_HEADLINES; i++) {
        if (headlineScrolls[i] != NULL) {
            headlineScrolls[i]->scroll_Active(STOP_SCROLL);
        }
    }
}

// Button event handler: trigger an immediate news update on button press.
void buttonNewsTickerHandler(button_event_t button_Data) {
    if (button_Data.type == BUTTON_PRESSED) {
        xSemaphoreGive(googleNewsupdateSem);
    }
}

// BLE Command Functions

// Command to manually update news headlines.
void showLatestNews(DynamicJsonDocument& dCommand) {
    uint8_t cmdNumber = dCommand["app_command"];
    xSemaphoreGive(googleNewsupdateSem);
    //ble_Application_Command_Respond_Success(cmdNumber, pdPASS);
}

// Command to set the news update interval (in milliseconds).
void setNewsUpdateInterval(DynamicJsonDocument& dCommand) {
    uint8_t cmdNumber = dCommand["app_command"];
    uint32_t interval = dCommand["interval"];
    googleNewsUpdateInterval = interval;
    if (xTimerIsTimerActive(googleNewsUpdateTimer_H) == pdTRUE)
        xTimerStop(googleNewsUpdateTimer_H, 0);
    xTimerChangePeriod(googleNewsUpdateTimer_H, pdMS_TO_TICKS(googleNewsUpdateInterval), 0);
    xTimerStart(googleNewsUpdateTimer_H, 0);
    //ble_Application_Command_Respond_Success(cmdNumber, pdPASS);
}

// Command to update the API key for news fetching.
void setNewsAPIKey(DynamicJsonDocument& dCommand) {
    uint8_t cmdNumber = dCommand["app_command"];
    googleNewsAPIKey = String(dCommand["api_key"].as<const char*>());
    //ble_Application_Command_Respond_Success(cmdNumber, pdPASS);
}

// Command to update the language code for the news headlines.
void setNewsLanguage(DynamicJsonDocument& dCommand) {
    uint8_t cmdNumber = dCommand["app_command"];
    googleNewsLanguage = String(dCommand["language"].as<const char*>());
    //ble_Application_Command_Respond_Success(cmdNumber, pdPASS);
}

#include "mtb_engine.h"
#include <HTTPClient.h>
#include "FS.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "mtb_text_scroll.h"
#include "my_secret_keys.h"

// Global variables and handles
EXT_RAM_BSS_ATTR SemaphoreHandle_t googleNewsupdateSem = NULL;
EXT_RAM_BSS_ATTR TimerHandle_t googleNewsUpdateTimer_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t googleNews_Task_H = NULL;
void googleNews_App_Task(void *);

uint32_t googleNewsUpdateInterval = 60000; // Update every 60 seconds by default
String googleNewsAPIKey = String(GNewsAPIKey); // Replace with your actual GNews API token
String googleNewsLanguage = "en";  // Default language for headlines

// Pre-defined colors for each headline line (16-bit colors)
uint16_t headlineColors[] = { WHITE, CYAN, GREEN, YELLOW, ORANGE, MAGENTA, BLUE, RED };

#define MAX_HEADLINES 8
// Global array to hold Mtb_ScrollText_t objects for each headline line.
Mtb_ScrollText_t* headlineScrolls[MAX_HEADLINES] = {0};

// Forward declarations of BLE command functions
void showLatestNews(JsonDocument& dCommand);
void setNewsUpdateInterval(JsonDocument& dCommand);
void setNewsAPIKey(JsonDocument& dCommand);
void setNewsLanguage(JsonDocument& dCommand);


String fetchNewsHeadlines();
void displayNewsHeadlines(String headlines);


// Forward declaration for button event handler
void buttonNewsTickerHandler(button_event_t button_Data);

// Timer callback to trigger news updates.
void googleNews_TimerCallback(TimerHandle_t timer) {
    xSemaphoreGive(googleNewsupdateSem);
}

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *googleNews_App = new Mtb_Applications_StatusBar(googleNews_App_Task, &googleNews_Task_H, "Google News", {1,0}, 4096);

// Main task for the News Ticker app.
void googleNews_App_Task(void * dApplication) {
    Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
    
    // Register BLE command functions.
    THIS_APP->mtb_App_Set_Ble_Comm_Sv_Fns(showLatestNews, setNewsUpdateInterval, setNewsAPIKey, setNewsLanguage);
    
    // Set button and encoder handlers.

      THIS_APP->mtb_App_Set_EC11_Cb_Fns(buttonNewsTickerHandler, mtb_Brightness_Control);
    // Initialize app services.
    THIS_APP->mtb_App_Init();
    
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
    mtb_Delete_This_App(THIS_APP);
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
        JsonDocument doc;
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

// Helper function to display headlines using Mtb_ScrollText_t.
// Each headline is shown on its own line (y = line * 8) with a unique color.
void displayNewsHeadlines(String headlines) {
    // Clear the display before updating
    mtb_Panel_Fill_Screen(BLACK);
    
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
        // For each line, create a Mtb_ScrollText_t object if it doesn't already exist.
        // Use x=0, full width of 128, and y position as (line index * 8).
        if (headlineScrolls[headlineIndex] == NULL) {
            headlineScrolls[headlineIndex] = new Mtb_ScrollText_t(
                0, 
                headlineIndex * 8, 
                128,
                Terminal6x8, 
                headlineColors[headlineIndex % (sizeof(headlineColors) / sizeof(headlineColors[0]))], 
                20, 
                1
            );
        }
        // Set the scrolling text with the corresponding color.
        headlineScrolls[headlineIndex]->mtb_Scroll_This_Text(
            lineText, 
            headlineColors[headlineIndex % (sizeof(headlineColors) / sizeof(headlineColors[0]))]
        );
        headlineIndex++;
    }
    // Stop scrolling for any remaining Mtb_ScrollText_t objects if fewer headlines are available.
    for (int i = headlineIndex; i < MAX_HEADLINES; i++) {
        if (headlineScrolls[i] != NULL) {
            headlineScrolls[i]->mtb_Scroll_Active(STOP_SCROLL);
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
void showLatestNews(JsonDocument& dCommand) {
    xSemaphoreGive(googleNewsupdateSem);
}

// Command to set the news update interval (in milliseconds).
void setNewsUpdateInterval(JsonDocument& dCommand) {
    uint32_t interval = dCommand["interval"];
    googleNewsUpdateInterval = interval;
    if (xTimerIsTimerActive(googleNewsUpdateTimer_H) == pdTRUE)
        xTimerStop(googleNewsUpdateTimer_H, 0);
    xTimerChangePeriod(googleNewsUpdateTimer_H, pdMS_TO_TICKS(googleNewsUpdateInterval), 0);
    xTimerStart(googleNewsUpdateTimer_H, 0);
}

// Command to update the API key for news fetching.
void setNewsAPIKey(JsonDocument& dCommand) {
    googleNewsAPIKey = String(dCommand["api_key"].as<const char*>());
}

// Command to update the language code for the news headlines.
void setNewsLanguage(JsonDocument& dCommand) {
    googleNewsLanguage = String(dCommand["language"].as<const char*>());
}

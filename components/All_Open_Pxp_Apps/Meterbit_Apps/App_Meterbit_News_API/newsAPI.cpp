#include "mtb_engine.h"
#include <HTTPClient.h>
#include "FS.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "mtb_text_scroll.h"
#include "newsAPI.h"

// Global variables and handles
EXT_RAM_BSS_ATTR SemaphoreHandle_t newsAPI_UpdateSem = NULL;
EXT_RAM_BSS_ATTR TimerHandle_t newsAPI_UpdateTimer_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t newsAPI_Task_H = NULL;
void googleNews_App_Task(void *);

uint32_t newsAPI_UpdateInterval = 60000; // Update every 60 seconds by default
String newsAPI_APIKey = "get api key from https://newsapi.org/"; // Replace with your actual newsAPI key token
String newsAPI_Language = "en";  // Default language for headlines

// Pre-defined colors for each headline line (16-bit colors)
uint16_t newsHeadlineColors[] = { WHITE, CYAN, GREEN, YELLOW, ORANGE, MAGENTA, BLUE, RED };

#define MAX_HEADLINES 8
// Global array to hold Mtb_ScrollText_t objects for each headline line.
Mtb_ScrollText_t* newsHeadlineScrolls[MAX_HEADLINES] = {0};

// Forward declarations of BLE command functions
void showLatestNewsAPI(JsonDocument& dCommand);
void setNewsAPIUpdateInterval(JsonDocument& dCommand);
void setNewsAPI_APIKey(JsonDocument& dCommand);
void setNewsAPI_Language(JsonDocument& dCommand);


String fetchNewsAPI_Headlines();
void displayNewsAPI_Headlines(String headlines);


// Forward declaration for button event handler
void buttonNewsAPI_Handler(button_event_t button_Data);

// Timer callback to trigger news updates.
void newsAPI_TimerCallback(TimerHandle_t timer) {
    xSemaphoreGive(newsAPI_UpdateSem);
}

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *newsAPI_App = new Mtb_Applications_StatusBar(newsAPI_App_Task, &newsAPI_Task_H, "NewsAPI App", 10240);

// Main task for the News Ticker app.
void newsAPI_App_Task(void * dApplication) {
    Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
    
    // Register BLE command functions.
    thisApp->mtb_App_Set_Ble_Comm_Sv_Fns(showLatestNewsAPI, setNewsAPIUpdateInterval, setNewsAPI_APIKey, setNewsAPI_Language);
    
    // Set button and encoder handlers.
    thisApp->mtb_App_Set_EC11_Cb_Fns(buttonNewsAPI_Handler, mtb_Brightness_Control);

    // Initialize app services.
    thisApp->mtb_App_Init(mtb_Status_Bar_Clock_Sv);
    
    if (newsAPI_UpdateSem == NULL)
        newsAPI_UpdateSem = xSemaphoreCreateBinary();
    if (newsAPI_UpdateTimer_H == NULL)
        newsAPI_UpdateTimer_H = xTimerCreate("newsUpdateTimer", pdMS_TO_TICKS(newsAPI_UpdateInterval), pdTRUE, NULL, newsAPI_TimerCallback);
    
    if (newsAPI_UpdateInterval > 0)
        xTimerStart(newsAPI_UpdateTimer_H, 0);
    
    // Perform an initial news fetch and display.
    String headlines = fetchNewsAPI_Headlines();
    displayNewsAPI_Headlines(headlines);
    
    // Main loop: update headlines when the semaphore is given (by timer or button press).
    while (MTB_APP_IS_ACTIVE == pdTRUE) {
        if (xSemaphoreTake(newsAPI_UpdateSem, portMAX_DELAY) == pdTRUE) {
            headlines = fetchNewsAPI_Headlines();
            displayNewsAPI_Headlines(headlines);
        }
    }
    mtb_Delete_This_App(thisApp);
}

// This function uses GNews API to fetch the latest headlines.
// It returns a string with each headline separated by a newline.
String fetchNewsAPI_Headlines() {
    String headlines;
    HTTPClient http;
    String apiUrl = "get the endpoints from https://newsapi.org/" + newsAPI_APIKey + "&lang=" + newsAPI_Language;
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
void displayNewsAPI_Headlines(String headlines) {
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
        if (newsHeadlineScrolls[headlineIndex] == NULL) {
            newsHeadlineScrolls[headlineIndex] = new Mtb_ScrollText_t(
                0, 
                headlineIndex * 8, 
                128,
                Terminal6x8, 
                newsHeadlineColors[headlineIndex % (sizeof(newsHeadlineColors) / sizeof(newsHeadlineColors[0]))], 
                20, 
                1
            );
        }
        // Set the scrolling text with the corresponding color.
        newsHeadlineScrolls[headlineIndex]->mtb_Scroll_This_Text(
            lineText, 
            newsHeadlineColors[headlineIndex % (sizeof(newsHeadlineColors) / sizeof(newsHeadlineColors[0]))]
        );
        headlineIndex++;
    }
    // Stop scrolling for any remaining Mtb_ScrollText_t objects if fewer headlines are available.
    for (int i = headlineIndex; i < MAX_HEADLINES; i++) {
        if (newsHeadlineScrolls[i] != NULL) {
            newsHeadlineScrolls[i]->mtb_Scroll_Active(STOP_SCROLL);
        }
    }
}

// Button event handler: trigger an immediate news update on button press.
void buttonNewsAPI_Handler(button_event_t button_Data) {
    if (button_Data.type == BUTTON_PRESSED) {
        xSemaphoreGive(newsAPI_UpdateSem);
    }
}

// BLE Command Functions

// Command to manually update news headlines.
void showLatestNewsAPI(JsonDocument& dCommand) {
    uint8_t cmdNumber = dCommand["app_command"];
    xSemaphoreGive(newsAPI_UpdateSem);
    //mtb_Ble_App_Cmd_Respond_Success(cmdNumber, pdPASS);
}

// Command to set the news update interval (in milliseconds).
void setNewsAPIUpdateInterval(JsonDocument& dCommand) {
    uint8_t cmdNumber = dCommand["app_command"];
    uint32_t interval = dCommand["interval"];
    newsAPI_UpdateInterval = interval;
    if (xTimerIsTimerActive(newsAPI_UpdateTimer_H) == pdTRUE)
        xTimerStop(newsAPI_UpdateTimer_H, 0);
    xTimerChangePeriod(newsAPI_UpdateTimer_H, pdMS_TO_TICKS(newsAPI_UpdateInterval), 0);
    xTimerStart(newsAPI_UpdateTimer_H, 0);
    //mtb_Ble_App_Cmd_Respond_Success(cmdNumber, pdPASS);
}

// Command to update the API key for news fetching.
void setNewsAPI_APIKey(JsonDocument& dCommand) {
    uint8_t cmdNumber = dCommand["app_command"];
    newsAPI_APIKey = String(dCommand["api_key"].as<const char*>());
    //mtb_Ble_App_Cmd_Respond_Success(cmdNumber, pdPASS);
}

// Command to update the language code for the news headlines.
void setNewsAPI_Language(JsonDocument& dCommand) {
    uint8_t cmdNumber = dCommand["app_command"];
    newsAPI_Language = String(dCommand["language"].as<const char*>());
    //mtb_Ble_App_Cmd_Respond_Success(cmdNumber, pdPASS);
}

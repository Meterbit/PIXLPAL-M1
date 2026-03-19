#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mtb_engine.h"
#include "mtb_text_scroll.h"
#include "LittleFS.h"
#include "mtb_buzzer.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "googleAuth.h"
#include "mtb_graphics.h"
#include "my_secret_keys.h"


static const char *TAG = "PXP_GOOGLE_CAL";

struct GoogleCal_Data_t {
char refreshToken[250] = {0};
bool showEvents = true;
bool showTasks = true;
bool showHolidays = true;
uint16_t themeColor = 0x4249; // Default to OUTER_SPACE color
};

EXT_RAM_BSS_ATTR GoogleCal_Data_t userGoogleCal;

// === User Settings ===
// Modify these flags to show/hide specific data
// === User Preferences ===
// User Display Settings
bool showEventTitle     = true;
bool showEventTime      = true;
bool showEventStatus  = true;
bool showEventAttendees = true;
bool showEventDescription = false;

bool showTaskTitle      = true;
bool showTaskDue        = true;
bool showTaskStatus     = true;
bool showTaskNotes      = true;
// ======================
  EXT_RAM_BSS_ATTR Mtb_FixedText_t* event_Task_Name;
  
  EXT_RAM_BSS_ATTR Mtb_ScrollText_t* event_Task_Title_1;
  EXT_RAM_BSS_ATTR Mtb_ScrollText_t* event_Task_Title_2;

  EXT_RAM_BSS_ATTR Mtb_FixedText_t* event_Task_Date_1;
  EXT_RAM_BSS_ATTR Mtb_FixedText_t* event_Task_Date_2;

  EXT_RAM_BSS_ATTR Mtb_FixedText_t* event_Task_Time_1;
  EXT_RAM_BSS_ATTR Mtb_FixedText_t* event_Task_Time_2;

  EXT_RAM_BSS_ATTR Mtb_FixedText_t* event_Task_Status_1;
  EXT_RAM_BSS_ATTR Mtb_FixedText_t* event_Task_Status_2;
 
  void fetchAllCalendarEvents(const String& accessToken);
  void fetchTasks(const String& accessToken);
  String mtb_Get_Current_Time_RFC3339();
  void google_Cal_Theme(void);
  String mtb_Url_Encode(const char* str);

void googleCalButtonControl(button_event_t){}
//*************************************************************************************************** */

  EXT_RAM_BSS_ATTR TaskHandle_t googleCal_Task_H = NULL;
  //TaskHandle_t screenUpdates_Task_H = NULL; 
  void googleCal_App_Task(void *);
  //void performScreenUpdate_Task( void * pvParameters );

  //*************************************************************************************************** */
  void link_GoogleCal(JsonDocument&);
  void get_GoogleCal_Refresh_Token(JsonDocument&);
  void show_GoogleCal_Events(JsonDocument& dCommand);
  void show_GoogleCal_Tasks(JsonDocument& dCommand);
  void show_GoogleCal_Holidays(JsonDocument& dCommand);
  void set_GoogleCal_ThemeColor(JsonDocument& dCommand);
//*************************************************************************************************** */

  //Mtb_Services *googleCalScreenUpdate_Sv = new Mtb_Services(performScreenUpdate_Task, &screenUpdates_Task_H, "screenUpdates", 10240, pdTRUE);
  Mtb_Applications_StatusBar *google_Calendar_App = new Mtb_Applications_StatusBar(googleCal_App_Task, &googleCal_Task_H, "googleCal App", 10240); // Review down this stack size later.

//THIS IS THE APPLICATION IMPLEMENTATION ***************************************************************************
void  googleCal_App_Task(void* dApplication){
  Mtb_Applications *thisApp = (Mtb_Applications *) dApplication;
  thisApp->mtb_App_Set_EC11_Cb_Fns(googleCalButtonControl, mtb_Brightness_Control);

  thisApp->mtb_App_Set_Ble_Comm_Sv_Fns(link_GoogleCal, get_GoogleCal_Refresh_Token, show_GoogleCal_Events, show_GoogleCal_Tasks, show_GoogleCal_Holidays, set_GoogleCal_ThemeColor);
  thisApp->mtb_App_Init(mtb_Status_Bar_Clock_Sv);
  //**************************************************************************************************************************************************************** */
  
  event_Task_Title_1 = new Mtb_ScrollText_t(12, 24, 113, Terminal6x8, CYAN, 10, 0xFFFF, 15000);
  event_Task_Title_2 = new Mtb_ScrollText_t(12, 44, 113, Terminal6x8, CYAN, 10, 0xFFFF, 15000);

  event_Task_Date_1 = new Mtb_FixedText_t(11, 35, Terminal4x6, LEMON_YELLOW);
  event_Task_Date_2 = new Mtb_FixedText_t(11, 55, Terminal4x6, LEMON_YELLOW);

  event_Task_Time_1 = new Mtb_FixedText_t(90, 35, Terminal4x6, SANDY_BROWN);
  event_Task_Time_2 = new Mtb_FixedText_t(90, 55, Terminal4x6, SANDY_BROWN);

  event_Task_Status_1 = new Mtb_FixedText_t(90, 35, Terminal4x6, SANDY_BROWN);
  event_Task_Status_2 = new Mtb_FixedText_t(80, 55, Terminal4x6, SANDY_BROWN);
  
  String googleCalendarRefreshTokener;

  userGoogleCal = (GoogleCal_Data_t){
        "no_Refresh_Token_Saved_Yet"
    };
    
  mtb_Read_Nvs_Struct("googleCalData", &userGoogleCal, sizeof(GoogleCal_Data_t));
  event_Task_Name = new Mtb_FixedText_t(20, 12, Terminal6x8, BLACK, userGoogleCal.themeColor);  
  google_Cal_Theme();

  //mtb_Draw_Local_Png({"/batIcons/googleEvent.png", 3, 11});
//######################################################################################### */
  while (MTB_APP_IS_ACTIVE == pdTRUE){

  while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);
  
  ESP_LOGI(TAG, "The refreshToken is: %s\n", userGoogleCal.refreshToken);
  
  googleCalendarRefreshTokener = String(userGoogleCal.refreshToken);

  String accessToken = getAccessToken(googleClient_ID, googleClient_SECRET, googleCalendarRefreshTokener);
  if (accessToken.isEmpty()) {
    ESP_LOGI(TAG, "Unable to retrieve access token.\n");
  } else {
      fetchAllCalendarEvents(accessToken);
      fetchTasks(accessToken); 
  }

    while (MTB_APP_IS_ACTIVE == pdTRUE){
      delay(100);
    }

  }

  delete event_Task_Name;
  
  delete event_Task_Title_1;
  delete event_Task_Title_2;

  delete event_Task_Date_1;
  delete event_Task_Date_2;

  delete event_Task_Time_1;
  delete event_Task_Time_2;

  delete event_Task_Status_1;
  delete event_Task_Status_2;

  mtb_Delete_This_App(thisApp);
}


//###########################################################################################################
void fetchEventsForCalendar(const String& accessToken, const char* calendarId) {
  HTTPClient http;
  String currentTime = mtb_Get_Current_Time_RFC3339();

  String url = "https://www.googleapis.com/calendar/v3/calendars/";
  url += mtb_Url_Encode(calendarId);  // URL-encode calendarId (e.g. contains @ symbol)
  url += "/events?maxResults=5&orderBy=startTime&singleEvents=true&timeMin=" + currentTime;

  http.begin(url);
  http.addHeader("Authorization", "Bearer " + accessToken);

  int code = http.GET();
  if (code != 200) {
    ESP_LOGI(TAG, "❌ Failed to fetch events for calendar %s: HTTP %d\n", calendarId, code);
    http.end();
    return;
  }

  JsonDocument doc;
  deserializeJson(doc, http.getString());
  JsonArray events = doc["items"].as<JsonArray>();

  for (JsonObject event : events) {
    ESP_LOGI(TAG, "📌 Title: %s\n", event["summary"] | "No Title");
    event_Task_Title_1->mtb_Scroll_This_Text(event["summary"] | "No Title");
    event_Task_Title_2->mtb_Scroll_This_Text(event["summary"] | "No Title");

    if (showEventTime) {
      ESP_LOGI(TAG, "🕒 Time: %s to %s\n",
             (const char*)(event["start"]["dateTime"] | event["start"]["date"]),
             (const char*)(event["end"]["dateTime"] | event["end"]["date"]));
      event_Task_Date_1->mtb_Write_String(mtb_Format_Iso_Date((const char*)(event["start"]["dateTime"] | event["start"]["date"])));
      event_Task_Date_2->mtb_Write_String(mtb_Format_Iso_Date((const char*)(event["start"]["dateTime"] | event["start"]["date"])));

      event_Task_Time_1->mtb_Write_String(mtb_Format_Iso_Time((const char*)(event["start"]["dateTime"] | event["start"]["date"])));
      event_Task_Time_2->mtb_Write_String(mtb_Format_Iso_Time((const char*)(event["start"]["dateTime"] | event["start"]["date"])));
    }

    // if (showEventStatus) {
    //   ESP_LOGI(TAG, "📍 Location: %s\n", event["location"] | "N/A");
    //   event_Task_Status_1.mtb_Write_String(event["status"] | "No Status");
    // }

    if (showEventAttendees && event.containsKey("attendees")) {
      ESP_LOGI(TAG, "👥 Attendees:\n");
      // for (JsonObject att : event["attendees"].as<JsonArray>()) {
      //   ESP_LOGI(TAG, " - %s\n", att["email"].as<const char*>());
      // }
    }

    if (showEventDescription) {
      ESP_LOGI(TAG, "📝 Description: %s\n", event["description"] | "None");
    }

    ESP_LOGI(TAG, "----\n");
  }

  http.end();
}


void fetchAllCalendarEvents(const String& accessToken) {
  HTTPClient http;
  JsonDocument doc;

  // Step 1: Fetch calendar list
  http.begin("https://www.googleapis.com/calendar/v3/users/me/calendarList");
  http.addHeader("Authorization", "Bearer " + accessToken);

  int code = http.GET();
  if (code != 200) {
    ESP_LOGI(TAG, "❌ Failed to fetch calendar list: HTTP %d\n", code);
    http.end();
    return;
  }

  DeserializationError err = deserializeJson(doc, http.getString());
  if (err) {
    ESP_LOGI(TAG, "❌ Failed to parse calendar list\n");
    http.end();
    return;
  }

  JsonArray items = doc["items"].as<JsonArray>();
  ESP_LOGI(TAG, "✅ Found %d calendars.\n", items.size());

  for (JsonObject cal : items) {
    const char* calendarId = cal["id"];
    const char* calendarName = cal["summary"];

    ESP_LOGI(TAG, "\n📅 Calendar: %s\n", calendarName);
    event_Task_Name->mtb_Write_String(calendarName);
    fetchEventsForCalendar(accessToken, calendarId);
  }

  http.end();
}


void fetchTasks(const String& accessToken) {
  HTTPClient http;
  http.begin("https://tasks.googleapis.com/tasks/v1/lists/@default/tasks?showCompleted=true");
  http.addHeader("Authorization", "Bearer " + accessToken);

  int code = http.GET();
  if (code == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    JsonArray items = doc["items"].as<JsonArray>();

    ESP_LOGI(TAG, "\nUpcoming Tasks:\n");
    for (JsonObject task : items) {
      const char* due = task["due"];
      const char* status = task["status"];
      if (due && strcmp(status, "completed") != 0) {
        // Parse due time to check if it's in the future
        struct tm dueTime = {};
        strptime(due, "%Y-%m-%dT%H:%M:%S.000Z", &dueTime);
        time_t dueEpoch = mktime(&dueTime);
        time_t now;
        time(&now);
        if (difftime(dueEpoch, now) < 0) continue;  // Skip past-due tasks
      }

      if (showTaskTitle) {
        ESP_LOGI(TAG, "📝 Title: %s\n", task["title"] | "No Title");
      }
      if (showTaskDue && due) {
        ESP_LOGI(TAG, "⏳ Due: %s ", due);
        time_t now;
        time(&now);
        struct tm dueTime = {};
        strptime(due, "%Y-%m-%dT%H:%M:%S.000Z", &dueTime);
        time_t dueEpoch = mktime(&dueTime);
        if (difftime(dueEpoch, now) < 0) {
          ESP_LOGI(TAG, "❗ Overdue");
        }
        ESP_LOGI(TAG, "\n");
      }
      if (showTaskStatus) {
        const char* stat = task["status"];
        if (strcmp(stat, "completed") == 0)
          ESP_LOGI(TAG, "✅ Completed\n");
        else
          ESP_LOGI(TAG, "❗ Pending\n");
      }
      if (showTaskNotes && task.containsKey("notes")) {
        ESP_LOGI(TAG, "📄 Notes: %s\n", task["notes"].as<const char*>());
      }
      ESP_LOGI(TAG, "----\n");
    }
  } else {
    ESP_LOGI(TAG, "Failed to get tasks: HTTP %d\n", code);
  }

  http.end();
}

  void google_Cal_Theme(void){
    mtb_Panel_Fill_Rect(0, 10, 127, 63, userGoogleCal.themeColor);   // Fill background color2
    mtb_Panel_Fill_Rect(2, 23, 125, 61, BLACK);           // Fill clock area color.
    mtb_Panel_Draw_Line(2, 42, 126, 42, userGoogleCal.themeColor);   // Draw inner borders1

    //mtb_Draw_Local_Png({"/batIcons/googleEvent.png", 3, 11});
    mtb_Draw_Local_Png({"/batIcons/aiResp.png", 3, 24});
    mtb_Draw_Local_Png({"/batIcons/aiResp.png", 3, 44});
    mtb_Draw_Local_Png({"/batIcons/dateSmall.png", 4, 35});
    mtb_Draw_Local_Png({"/batIcons/dateSmall.png", 4, 55});
    mtb_Draw_Local_Png({"/batIcons/timeSmall.png", 83, 35});
    mtb_Draw_Local_Png({"/batIcons/timeSmall.png", 83, 55});
  }

  //***************************************************************************************************
  void link_GoogleCal(JsonDocument& dCommand){
  }

  void get_GoogleCal_Refresh_Token(JsonDocument& dCommand){
    const char *refreshToken = dCommand["refreshToken"];
    strcpy(userGoogleCal.refreshToken, refreshToken);
    mtb_Write_Nvs_Struct("googleCalData", &userGoogleCal, sizeof(GoogleCal_Data_t));
    do_beep(CLICK_BEEP);
    statusBarNotif.mtb_Scroll_This_Text("GOOGLE CALENDAR LINK UPDATED. YOU MAY CLOSE THE BROWSER", GREEN_LIZARD);
  }

  void show_GoogleCal_Events(JsonDocument& dCommand){
    uint8_t setCycle = dCommand["showEvents"];

    mtb_Write_Nvs_Struct("googleCalData", &userGoogleCal, sizeof(GoogleCal_Data_t));
  }
  void show_GoogleCal_Tasks(JsonDocument& dCommand){
    uint8_t setCycle = dCommand["showTasks"];

    mtb_Write_Nvs_Struct("googleCalData", &userGoogleCal, sizeof(GoogleCal_Data_t));
  }
  void show_GoogleCal_Holidays(JsonDocument& dCommand){
    uint8_t setCycle = dCommand["showHoliday"];

    mtb_Write_Nvs_Struct("googleCalData", &userGoogleCal, sizeof(GoogleCal_Data_t));
  }

  void set_GoogleCal_ThemeColor(JsonDocument& dCommand){
    const char *color = dCommand["themeColor"];
    color += 4;
    userGoogleCal.themeColor = mtb_Panel_Color565(((uint8_t)((strtol(color,NULL,16) >> 16))), ((uint8_t)((strtol(color,NULL,16) >> 8))),((uint8_t)((strtol(color,NULL,16) >> 0))));
    google_Cal_Theme();
    event_Task_Name->backgroundColor = userGoogleCal.themeColor;

    mtb_Write_Nvs_Struct("googleCalData", &userGoogleCal, sizeof(GoogleCal_Data_t));
  }
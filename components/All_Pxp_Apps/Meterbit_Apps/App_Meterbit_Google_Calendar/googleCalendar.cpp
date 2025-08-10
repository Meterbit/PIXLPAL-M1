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
#include "googleCalendar.h"
#include "mtb_graphics.h"


static const char *TAG = "PXP_GOOGLE_CAL";

GoogleCal_Data_t userGoogleCal = {
    "no_Refresh_Token_Saved_Yet"
};

const String CLIENT_ID = "1073159638977-j08khjg9s60i37g5386odt2vps0oko97.apps.googleusercontent.com";
const String CLIENT_SECRET = "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";

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
  FixedText_t* event_Task_Name;
  
  ScrollText_t* event_Task_Title_1 = new ScrollText_t(12, 24, 113, CYAN, 10, 0xFFFF, Terminal6x8, 15000);
  ScrollText_t* event_Task_Title_2 = new ScrollText_t(12, 44, 113, CYAN, 10, 0xFFFF, Terminal6x8, 15000);

  FixedText_t* event_Task_Date_1 = new FixedText_t(11, 35, Terminal4x6, LEMON_YELLOW);
  FixedText_t* event_Task_Date_2 = new FixedText_t(11, 55, Terminal4x6, LEMON_YELLOW);

  FixedText_t* event_Task_Time_1 = new FixedText_t(90, 35, Terminal4x6, SANDY_BROWN);
  FixedText_t* event_Task_Time_2 = new FixedText_t(90, 55, Terminal4x6, SANDY_BROWN);

  FixedText_t* event_Task_Status_1 = new FixedText_t(90, 35, Terminal4x6, SANDY_BROWN);
  FixedText_t* event_Task_Status_2 = new FixedText_t(80, 55, Terminal4x6, SANDY_BROWN);
 
  void fetchAllCalendarEvents(const String& accessToken);
  void fetchTasks(const String& accessToken);
  String getCurrentTimeRFC3339();
  void printPixAnimClkInterface(void);
  String urlencode(const char* str);

void googleCalButtonControl(button_event_t){}
//*************************************************************************************************** */

  TaskHandle_t googleCal_Task_H = NULL;
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

  //Services *googleCalScreenUpdate_Sv = new Services(performScreenUpdate_Task, &screenUpdates_Task_H, "screenUpdates", 10240, pdTRUE);
  Applications_StatusBar *google_Calendar_App = new Applications_StatusBar(googleCal_App_Task, &googleCal_Task_H, "googleCal App", 10240); // Review down this stack size later.

//THIS IS THE APPLICATION IMPLEMENTATION ***************************************************************************
void  googleCal_App_Task(void* dApplication){
  Applications *thisApp = (Applications *) dApplication;
  thisApp->app_EncoderFn_ptr = brightnessControl;
  thisApp->app_ButtonFn_ptr = googleCalButtonControl;
  ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(link_GoogleCal, get_GoogleCal_Refresh_Token, show_GoogleCal_Events, show_GoogleCal_Tasks, show_GoogleCal_Holidays, set_GoogleCal_ThemeColor);
  appsInitialization(thisApp, statusBarClock_Sv);
  //**************************************************************************************************************************************************************** */

  String googleCalendarRefreshTokener;
  read_struct_from_nvs("googleCalData", &userGoogleCal, sizeof(GoogleCal_Data_t));
  event_Task_Name = new FixedText_t(20, 12, Terminal6x8, BLACK, userGoogleCal.themeColor);  
  printPixAnimClkInterface();

  //drawLocalPNG({"/batIcons/googleEvent.png", 3, 11});
//######################################################################################### */
  while (THIS_APP_IS_ACTIVE == pdTRUE){

  while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);
  
  printf("The refreshToken is: %s\n", userGoogleCal.refreshToken);
  
  googleCalendarRefreshTokener = String(userGoogleCal.refreshToken);

  String accessToken = getAccessToken(CLIENT_ID, CLIENT_SECRET, googleCalendarRefreshTokener);
  if (accessToken.isEmpty()) {
    printf("Unable to retrieve access token.\n");
  } else {
      fetchAllCalendarEvents(accessToken);
      fetchTasks(accessToken); 
  }

    while (THIS_APP_IS_ACTIVE == pdTRUE){
      delay(100);
    }

  }

  kill_This_App(thisApp);
}


//###########################################################################################################
void fetchEventsForCalendar(const String& accessToken, const char* calendarId) {
  HTTPClient http;
  String currentTime = getCurrentTimeRFC3339();

  String url = "https://www.googleapis.com/calendar/v3/calendars/";
  url += urlencode(calendarId);  // URL-encode calendarId (e.g. contains @ symbol)
  url += "/events?maxResults=5&orderBy=startTime&singleEvents=true&timeMin=" + currentTime;

  http.begin(url);
  http.addHeader("Authorization", "Bearer " + accessToken);

  int code = http.GET();
  if (code != 200) {
    printf("❌ Failed to fetch events for calendar %s: HTTP %d\n", calendarId, code);
    http.end();
    return;
  }

  JsonDocument doc;
  deserializeJson(doc, http.getString());
  JsonArray events = doc["items"].as<JsonArray>();

  for (JsonObject event : events) {
    printf("📌 Title: %s\n", event["summary"] | "No Title");
    event_Task_Title_1->scroll_This_Text(event["summary"] | "No Title");
    event_Task_Title_2->scroll_This_Text(event["summary"] | "No Title");

    if (showEventTime) {
      printf("🕒 Time: %s to %s\n",
             (const char*)(event["start"]["dateTime"] | event["start"]["date"]),
             (const char*)(event["end"]["dateTime"] | event["end"]["date"]));
      event_Task_Date_1->writeString(formatIsoDate((const char*)(event["start"]["dateTime"] | event["start"]["date"])));
      event_Task_Date_2->writeString(formatIsoDate((const char*)(event["start"]["dateTime"] | event["start"]["date"])));

      event_Task_Time_1->writeString(formatIsoTime((const char*)(event["start"]["dateTime"] | event["start"]["date"])));
      event_Task_Time_2->writeString(formatIsoTime((const char*)(event["start"]["dateTime"] | event["start"]["date"])));
    }

    // if (showEventStatus) {
    //   printf("📍 Location: %s\n", event["location"] | "N/A");
    //   event_Task_Status_1.writeString(event["status"] | "No Status");
    // }

    if (showEventAttendees && event.containsKey("attendees")) {
      printf("👥 Attendees:\n");
      for (JsonObject att : event["attendees"].as<JsonArray>()) {
        printf(" - %s\n", att["email"].as<const char*>());
      }
    }

    if (showEventDescription) {
      printf("📝 Description: %s\n", event["description"] | "None");
    }

    printf("----\n");
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
    printf("❌ Failed to fetch calendar list: HTTP %d\n", code);
    http.end();
    return;
  }

  DeserializationError err = deserializeJson(doc, http.getString());
  if (err) {
    printf("❌ Failed to parse calendar list\n");
    http.end();
    return;
  }

  JsonArray items = doc["items"].as<JsonArray>();
  printf("✅ Found %d calendars.\n", items.size());

  for (JsonObject cal : items) {
    const char* calendarId = cal["id"];
    const char* calendarName = cal["summary"];

    printf("\n📅 Calendar: %s\n", calendarName);
    event_Task_Name->writeString(calendarName);
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

    printf("\nUpcoming Tasks:\n");
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
        printf("📝 Title: %s\n", task["title"] | "No Title");
      }
      if (showTaskDue && due) {
        printf("⏳ Due: %s ", due);
        time_t now;
        time(&now);
        struct tm dueTime = {};
        strptime(due, "%Y-%m-%dT%H:%M:%S.000Z", &dueTime);
        time_t dueEpoch = mktime(&dueTime);
        if (difftime(dueEpoch, now) < 0) {
          printf("❗ Overdue");
        }
        printf("\n");
      }
      if (showTaskStatus) {
        const char* stat = task["status"];
        if (strcmp(stat, "completed") == 0)
          printf("✅ Completed\n");
        else
          printf("❗ Pending\n");
      }
      if (showTaskNotes && task.containsKey("notes")) {
        printf("📄 Notes: %s\n", task["notes"].as<const char*>());
      }
      printf("----\n");
    }
  } else {
    printf("Failed to get tasks: HTTP %d\n", code);
  }

  http.end();
}

  void printPixAnimClkInterface(void){
    dma_display->fillRect(0, 10, 128, 63, userGoogleCal.themeColor);   // Fill background color2
    dma_display->fillRect(2, 23, 124, 39, BLACK);           // Fill clock area color.
    dma_display->drawLine(2, 42, 125, 42, userGoogleCal.themeColor);   // Draw inner borders1

    //drawLocalPNG({"/batIcons/googleEvent.png", 3, 11});
    drawLocalPNG({"/batIcons/aiResp.png", 3, 24});
    drawLocalPNG({"/batIcons/aiResp.png", 3, 44});
    drawLocalPNG({"/batIcons/dateSmall.png", 4, 35});
    drawLocalPNG({"/batIcons/dateSmall.png", 4, 55});
    drawLocalPNG({"/batIcons/timeSmall.png", 83, 35});
    drawLocalPNG({"/batIcons/timeSmall.png", 83, 55});
  }

  //***************************************************************************************************
  void link_GoogleCal(JsonDocument& dCommand){
    uint8_t cmd = dCommand["app_command"];
    mtb_Ble_App_Cmd_Respond_Success(googleCalendarAppRoute, cmd, pdPASS);
  }

  void get_GoogleCal_Refresh_Token(JsonDocument& dCommand){
    uint8_t cmd = dCommand["app_command"];
    const char *refreshToken = dCommand["refreshToken"];
    strcpy(userGoogleCal.refreshToken, refreshToken);
    write_struct_to_nvs("googleCalData", &userGoogleCal, sizeof(GoogleCal_Data_t));
    do_beep(CLICK_BEEP);
    statusBarNotif.scroll_This_Text("GOOGLE CALENDAR LINK UPDATED. YOU MAY CLOSE THE BROWSER", GREEN_LIZARD);
    mtb_Ble_App_Cmd_Respond_Success(googleCalendarAppRoute, cmd, pdPASS);
  }

  void show_GoogleCal_Events(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    uint8_t setCycle = dCommand["showEvents"];


    write_struct_to_nvs("googleCalData", &userGoogleCal, sizeof(GoogleCal_Data_t));
    mtb_Ble_App_Cmd_Respond_Success(googleCalendarAppRoute, cmdNumber, pdPASS);
  }
  void show_GoogleCal_Tasks(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    uint8_t setCycle = dCommand["showTasks"];


    write_struct_to_nvs("googleCalData", &userGoogleCal, sizeof(GoogleCal_Data_t));
    mtb_Ble_App_Cmd_Respond_Success(googleCalendarAppRoute, cmdNumber, pdPASS);
  }
  void show_GoogleCal_Holidays(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    uint8_t setCycle = dCommand["showHoliday"];


    write_struct_to_nvs("googleCalData", &userGoogleCal, sizeof(GoogleCal_Data_t));
    mtb_Ble_App_Cmd_Respond_Success(googleCalendarAppRoute, cmdNumber, pdPASS);
  }

  void set_GoogleCal_ThemeColor(JsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    const char *color = dCommand["themeColor"];
    color += 4;
    userGoogleCal.themeColor = dma_display->color565(((uint8_t)((strtol(color,NULL,16) >> 16))), ((uint8_t)((strtol(color,NULL,16) >> 8))),((uint8_t)((strtol(color,NULL,16) >> 0))));
    printPixAnimClkInterface();
    event_Task_Name->backgroundColor = userGoogleCal.themeColor;

    write_struct_to_nvs("googleCalData", &userGoogleCal, sizeof(GoogleCal_Data_t));
    mtb_Ble_App_Cmd_Respond_Success(googleCalendarAppRoute, cmdNumber, pdPASS);
  }
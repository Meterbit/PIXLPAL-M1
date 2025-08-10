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
#include "outlookAuth.h"
#include "outlookCalendar.h"
#include "mtb_graphics.h"

static const char *TAG = "PXP_OUTLOOK_CAL";

const String CLIENT_ID = "";
const String CLIENT_SECRET = "";

// === User Settings ===
// Modify these flags to show/hide specific data
// === User Preferences ===
// User Display Settings
bool showOutlookEventTitle     = true;
bool showOutlookEventTime      = true;
bool showOutlookEventStatus  = true;
bool showOutlookEventAttendees = true;
bool showOutlookEventDescription = false;

bool showOutlookTaskTitle      = true;
bool showOutlookTaskDue        = true;
bool showOutlookTaskStatus     = true;
bool showOutlookTaskNotes      = true;
// ======================
  FixedText_t* outlookEvent_Task_Name = new FixedText_t(20, 12, Terminal6x8, BLACK, OUTER_SPACE);
  
  ScrollText_t* outlookEvent_Task_Title_1 = new ScrollText_t(12, 24, 113, CYAN, 10, 0xFFFF, Terminal6x8, 15000);
  ScrollText_t* outlookEvent_Task_Title_2 = new ScrollText_t(12, 44, 113, CYAN, 10, 0xFFFF, Terminal6x8, 15000);

  FixedText_t* outlookEvent_Task_Date_1 = new FixedText_t(11, 35, Terminal4x6, LEMON_YELLOW);
  FixedText_t* outlookEvent_Task_Date_2 = new FixedText_t(11, 55, Terminal4x6, LEMON_YELLOW);

  FixedText_t* outlookEvent_Task_Time_1 = new FixedText_t(90, 35, Terminal4x6, SANDY_BROWN);
  FixedText_t* outlookEvent_Task_Time_2 = new FixedText_t(90, 55, Terminal4x6, SANDY_BROWN);

  FixedText_t* outlookEvent_Task_Status_1 = new FixedText_t(90, 35, Terminal4x6, SANDY_BROWN);
  FixedText_t* outlookEvent_Task_Status_2 = new FixedText_t(80, 55, Terminal4x6, SANDY_BROWN);
 
  void fetchEventsForOutlookCalendar(const String& accessToken, const char* calendarId);
  void fetchAllOutlookCalendarEvents(const String& accessToken);
  void fetchOutlookTasks(const String& accessToken);
  String getCurrentTimeRFC3339();
  void printOutlookCalThm(void);
  String urlencode(const char* str);

void outlookCalButtonControl(button_event_t){}
//*************************************************************************************************** */

  OutlookCal_Data_t userOutlookCal = {
      "no_Refresh_Token_Saved_Yet"
  };

  TaskHandle_t outlookCal_Task_H = NULL;
  //TaskHandle_t screenUpdates_Task_H = NULL; 
  void outlookCal_App_Task(void *);
  //void performScreenUpdate_Task( void * pvParameters );

  //*************************************************************************************************** */
  void link_OutlookCal(DynamicJsonDocument&);
  void get_OutlookCal_Refresh_Token(DynamicJsonDocument&);
  void show_OutlookCal_Events(DynamicJsonDocument& dCommand);
  void show_OutlookCal_Tasks(DynamicJsonDocument& dCommand);
  void show_OutlookCal_Holidays(DynamicJsonDocument& dCommand);

  Applications_StatusBar *outlook_Calendar_App = new Applications_StatusBar(outlookCal_App_Task, &outlookCal_Task_H, "outlookCal App", 10240); // Review down this stack size later.

//THIS IS THE APPLICATION IMPLEMENTATION ***************************************************************************
void  outlookCal_App_Task(void* dApplication){
  Applications *thisApp = (Applications *) dApplication;
  thisApp->app_EncoderFn_ptr = brightnessControl;
  thisApp->app_ButtonFn_ptr = outlookCalButtonControl;
  ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(link_OutlookCal, get_OutlookCal_Refresh_Token, show_OutlookCal_Events, show_OutlookCal_Tasks, show_OutlookCal_Holidays);
  appsInitialization(thisApp, statusBarClock_Sv);
  //**************************************************************************************************************************************************************** */
  String outlookCalendarRefreshTokener;
  printOutlookCalThm();
  drawLocalPNG({"/batIcons/outlookEvent.png", 3, 11});
  drawLocalPNG({"/batIcons/aiResp.png", 3, 24});
  drawLocalPNG({"/batIcons/aiResp.png", 3, 44});
  drawLocalPNG({"/batIcons/dateSmall.png", 4, 35});
  drawLocalPNG({"/batIcons/dateSmall.png", 4, 55});
  drawLocalPNG({"/batIcons/timeSmall.png", 83, 35});
  drawLocalPNG({"/batIcons/timeSmall.png", 83, 55});
  //drawLocalPNG({"/batIcons/outlookEvent.png", 3, 11});
//######################################################################################### */
  while (THIS_APP_IS_ACTIVE == pdTRUE){

  while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);
  
  read_struct_from_nvs("outlookCalData", &userOutlookCal, sizeof(OutlookCal_Data_t));
  printf("The refreshToken is: %s\n", userOutlookCal.refreshToken);
  
  outlookCalendarRefreshTokener = String(userOutlookCal.refreshToken);

  String accessToken = getAccessToken(CLIENT_ID, CLIENT_SECRET, outlookCalendarRefreshTokener);
  if (accessToken.isEmpty()) {
    printf("Unable to retrieve access token.\n");
  } else {
      fetchAllOutlookCalendarEvents(accessToken);
      fetchOutlookTasks(accessToken); 
  }


    while (THIS_APP_IS_ACTIVE == pdTRUE){
      delay(100);
    }

  }

  kill_This_App(thisApp);
}


// void performScreenUpdate_Task( void * d_Service ){
//       Services *thisServ = (Services *)d_Service;

//     while(THIS_SERV_IS_ACTIVE == pdTRUE){
//           //server.handleClient();
//     }
//     kill_This_Service(thisServ);
// }


void fetchEventsForOutlookCalendar(const String& accessToken, const char* calendarId) {
  HTTPClient http;
  String currentTime = getCurrentTimeRFC3339();

  String url = "https://www.outlook.apis.com/calendar/v3/calendars/";
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

  DynamicJsonDocument doc(8192);
  deserializeJson(doc, http.getString());
  JsonArray events = doc["items"].as<JsonArray>();

  for (JsonObject event : events) {
    printf("📌 Title: %s\n", event["summary"] | "No Title");
    outlookEvent_Task_Title_1->scroll_This_Text(event["summary"] | "No Title");
    outlookEvent_Task_Title_2->scroll_This_Text(event["summary"] | "No Title");

    if (showOutlookEventTime) {
      printf("🕒 Time: %s to %s\n",
             (const char*)(event["start"]["dateTime"] | event["start"]["date"]),
             (const char*)(event["end"]["dateTime"] | event["end"]["date"]));
      outlookEvent_Task_Date_1->writeString(formatIsoDate((const char*)(event["start"]["dateTime"] | event["start"]["date"])));
      outlookEvent_Task_Date_2->writeString(formatIsoDate((const char*)(event["start"]["dateTime"] | event["start"]["date"])));

      outlookEvent_Task_Time_1->writeString(formatIsoTime((const char*)(event["start"]["dateTime"] | event["start"]["date"])));
      outlookEvent_Task_Time_2->writeString(formatIsoTime((const char*)(event["start"]["dateTime"] | event["start"]["date"])));
    }

    // if (showEventStatus) {
    //   printf("📍 Location: %s\n", event["location"] | "N/A");
    //   event_Task_Status_1.writeString(event["status"] | "No Status");
    // }

    if (showOutlookEventAttendees && event.containsKey("attendees")) {
      printf("👥 Attendees:\n");
      for (JsonObject att : event["attendees"].as<JsonArray>()) {
        printf(" - %s\n", att["email"].as<const char*>());
      }
    }

    if (showOutlookEventDescription) {
      printf("📝 Description: %s\n", event["description"] | "None");
    }

    printf("----\n");
  }

  http.end();
}


void fetchAllOutlookCalendarEvents(const String& accessToken) {
  HTTPClient http;
  DynamicJsonDocument doc(8192);

  // Step 1: Fetch calendar list
  http.begin("https://www.apis.com/calendar/v3/users/me/calendarList");
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
    outlookEvent_Task_Name->writeString(calendarName);
    fetchEventsForOutlookCalendar(accessToken, calendarId);
  }

  http.end();
}


void fetchOutlookTasks(const String& accessToken) {
  HTTPClient http;
  http.begin("https://tasks.outlookapis.com/tasks/v1/lists/@default/tasks?showCompleted=true");
  http.addHeader("Authorization", "Bearer " + accessToken);

  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(8192);
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

      if (showOutlookTaskTitle) {
        printf("📝 Title: %s\n", task["title"] | "No Title");
      }
      if (showOutlookTaskDue && due) {
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
      if (showOutlookTaskStatus) {
        const char* stat = task["status"];
        if (strcmp(stat, "completed") == 0)
          printf("✅ Completed\n");
        else
          printf("❗ Pending\n");
      }
      if (showOutlookTaskNotes && task.containsKey("notes")) {
        printf("📄 Notes: %s\n", task["notes"].as<const char*>());
      }
      printf("----\n");
    }
  } else {
    printf("Failed to get tasks: HTTP %d\n", code);
  }

  http.end();
}

  void printOutlookCalThm(void){
    dma_display->fillRect(0, 10, 128, 63, OUTER_SPACE);   // Fill background color2
    dma_display->fillRect(2, 23, 124, 39, BLACK);           // Fill clock area color.
    dma_display->drawLine(2, 42, 125, 42, OUTER_SPACE);   // Draw inner borders1
  }

  //***************************************************************************************************
  void link_OutlookCal(DynamicJsonDocument& dCommand){
    uint8_t cmd = dCommand["app_command"];
    ble_Application_Command_Respond_Success(outlookCalendarAppRoute, cmd, pdPASS);
  }

  void get_OutlookCal_Refresh_Token(DynamicJsonDocument& dCommand){
    uint8_t cmd = dCommand["app_command"];
    const char *refreshToken = dCommand["refreshToken"];
    strcpy(userOutlookCal.refreshToken, refreshToken);
    write_struct_to_nvs("outlookCalData", &userOutlookCal, sizeof(OutlookCal_Data_t));
    do_beep(CLICK_BEEP);
    statusBarNotif.scroll_This_Text("OUTLOOK CALENDAR LINK UPDATED. YOU MAY CLOSE THE BROWSER", GREEN_LIZARD);
    ble_Application_Command_Respond_Success(outlookCalendarAppRoute, cmd, pdPASS);
  }



  void show_OutlookCal_Events(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    uint8_t setCycle = dCommand["showEvents"];


    write_struct_to_nvs("outlookCalData", &userOutlookCal, sizeof(OutlookCal_Data_t));
    ble_Application_Command_Respond_Success(outlookCalendarAppRoute, cmdNumber, pdPASS);
  }
  
  void show_OutlookCal_Tasks(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    uint8_t setCycle = dCommand["showTasks"];


    write_struct_to_nvs("outlookCalData", &userOutlookCal, sizeof(OutlookCal_Data_t));
    ble_Application_Command_Respond_Success(outlookCalendarAppRoute, cmdNumber, pdPASS);
  }

  void show_OutlookCal_Holidays(DynamicJsonDocument& dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    uint8_t setCycle = dCommand["showHoliday"];


    write_struct_to_nvs("outlookCalData", &userOutlookCal, sizeof(OutlookCal_Data_t));
    ble_Application_Command_Respond_Success(outlookCalendarAppRoute, cmdNumber, pdPASS);
  }
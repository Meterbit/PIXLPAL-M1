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
#include "my_secret_keys.h"

static const char *TAG = "OUTLOOK_CAL";

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
  EXT_RAM_BSS_ATTR Mtb_FixedText_t* outlookEvent_Task_Name;
  
  EXT_RAM_BSS_ATTR Mtb_ScrollText_t* outlookEvent_Task_Title_1;
  EXT_RAM_BSS_ATTR Mtb_ScrollText_t* outlookEvent_Task_Title_2;

  EXT_RAM_BSS_ATTR Mtb_FixedText_t* outlookEvent_Task_Date_1;
  EXT_RAM_BSS_ATTR Mtb_FixedText_t* outlookEvent_Task_Date_2;

  EXT_RAM_BSS_ATTR Mtb_FixedText_t* outlookEvent_Task_Time_1;
  EXT_RAM_BSS_ATTR Mtb_FixedText_t* outlookEvent_Task_Time_2;

  EXT_RAM_BSS_ATTR Mtb_FixedText_t* outlookEvent_Task_Status_1;
  EXT_RAM_BSS_ATTR Mtb_FixedText_t* outlookEvent_Task_Status_2;
 
  void fetchEventsForOutlookCalendar(const String& accessToken, const char* calendarId);
  void fetchAllOutlookCalendarEvents(const String& accessToken);
  void fetchOutlookTasks(const String& accessToken);
  void printOutlookCalThm(void);
  String mtb_Url_Encode(const char* str);

void outlookCalButtonControl(button_event_t){}
//*************************************************************************************************** */

  EXT_RAM_BSS_ATTR OutlookCal_Data_t userOutlookCal;

  TaskHandle_t outlookCal_Task_H = NULL;
  //TaskHandle_t screenUpdates_Task_H = NULL; 
  void outlookCal_App_Task(void *);
  //void performScreenUpdate_Task( void * pvParameters );

  //*************************************************************************************************** */
  void link_OutlookCal(JsonDocument&);
  void get_OutlookCal_Refresh_Token(JsonDocument&);
  void show_OutlookCal_Events(JsonDocument& dCommand);
  void show_OutlookCal_Tasks(JsonDocument& dCommand);
  void show_OutlookCal_Holidays(JsonDocument& dCommand);

  Mtb_Applications_StatusBar *outlook_Calendar_App = new Mtb_Applications_StatusBar(outlookCal_App_Task, &outlookCal_Task_H, "outlookCal App", {2,1}, 4096); // Review down this stack size later.

//THIS IS THE APPLICATION IMPLEMENTATION ***************************************************************************
void  outlookCal_App_Task(void* dApplication){
  Mtb_Applications *THIS_APP = (Mtb_Applications *) dApplication;
  THIS_APP->mtb_App_Set_EC11_Cb_Fns(outlookCalButtonControl, mtb_Brightness_Control);
  THIS_APP->mtb_App_Set_Ble_Comm_Fns(link_OutlookCal, get_OutlookCal_Refresh_Token, show_OutlookCal_Events, show_OutlookCal_Tasks, show_OutlookCal_Holidays);
  THIS_APP->mtb_App_Init();
  //**************************************************************************************************************************************************************** */
  outlookEvent_Task_Name = new Mtb_FixedText_t(20, 12, Terminal6x8, BLACK, OUTER_SPACE);
  
  outlookEvent_Task_Title_1 = new Mtb_ScrollText_t(12, 24, 113, Terminal6x8, CYAN, 10, 0xFFFF, 15000);
  outlookEvent_Task_Title_2 = new Mtb_ScrollText_t(12, 44, 113, Terminal6x8, CYAN, 10, 0xFFFF, 15000);

  outlookEvent_Task_Date_1 = new Mtb_FixedText_t(11, 35, Terminal4x6, LEMON_YELLOW);
  outlookEvent_Task_Date_2 = new Mtb_FixedText_t(11, 55, Terminal4x6, LEMON_YELLOW);

  outlookEvent_Task_Time_1 = new Mtb_FixedText_t(90, 35, Terminal4x6, SANDY_BROWN);
  outlookEvent_Task_Time_2 = new Mtb_FixedText_t(90, 55, Terminal4x6, SANDY_BROWN);

  outlookEvent_Task_Status_1 = new Mtb_FixedText_t(90, 35, Terminal4x6, SANDY_BROWN);
  outlookEvent_Task_Status_2 = new Mtb_FixedText_t(80, 55, Terminal4x6, SANDY_BROWN);
  
  String outlookCalendarRefreshTokener;
  printOutlookCalThm();
  mtb_Draw_Local_Png({"/batIcons/outlookEvent.png", 3, 11});
  mtb_Draw_Local_Png({"/batIcons/aiResp.png", 3, 24});
  mtb_Draw_Local_Png({"/batIcons/aiResp.png", 3, 44});
  mtb_Draw_Local_Png({"/batIcons/dateSmall.png", 4, 35});
  mtb_Draw_Local_Png({"/batIcons/dateSmall.png", 4, 55});
  mtb_Draw_Local_Png({"/batIcons/timeSmall.png", 83, 35});
  mtb_Draw_Local_Png({"/batIcons/timeSmall.png", 83, 55});
  //mtb_Draw_Local_Png({"/batIcons/outlookEvent.png", 3, 11});
//######################################################################################### */
  userOutlookCal = (OutlookCal_Data_t){
      "no_Refresh_Token_Saved_Yet"
  };

  while (THIS_APP_IS_ACTIVE == pdTRUE){
  while ((Mtb_Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);
  
  mtb_Read_Nvs_Struct("outlookCalData", &userOutlookCal, sizeof(OutlookCal_Data_t));
  ESP_LOGI(TAG, "The refreshToken is: %s\n", userOutlookCal.refreshToken);
  
  outlookCalendarRefreshTokener = String(userOutlookCal.refreshToken);

  String accessToken = getAccessToken(outlookClient_ID, outlookClient_SECRET, outlookCalendarRefreshTokener);
  if (accessToken.isEmpty()) {
    ESP_LOGI(TAG, "Unable to retrieve access token.\n");
  } else {
      fetchAllOutlookCalendarEvents(accessToken);
      fetchOutlookTasks(accessToken); 
  }


    while (THIS_APP_IS_ACTIVE == pdTRUE){
      delay(100);
    }

  }

  delete outlookEvent_Task_Name;
  
  delete outlookEvent_Task_Title_1;
  delete outlookEvent_Task_Title_2;

  delete outlookEvent_Task_Date_1;
  delete outlookEvent_Task_Date_2;

  delete outlookEvent_Task_Time_1;
  delete outlookEvent_Task_Time_2;

  delete outlookEvent_Task_Status_1;
  delete outlookEvent_Task_Status_2;


  mtb_Delete_This_App(THIS_APP);
}


// void performScreenUpdate_Task( void * d_Service ){
//       Mtb_Services *thisServ = (Mtb_Services *)d_Service;

//     while(MTB_SERV_IS_ACTIVE == pdTRUE){
//           //server.handleClient();
//     }
//     mtb_Delete_This_Service(thisServ);
// }


void fetchEventsForOutlookCalendar(const String& accessToken, const char* calendarId) {
  HTTPClient http;
  String currentTime = mtb_Get_Current_Time_RFC3339();

  String url = "https://www.outlook.apis.com/calendar/v3/calendars/";
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
    outlookEvent_Task_Title_1->mtb_Scroll_This_Text(event["summary"] | "No Title");
    outlookEvent_Task_Title_2->mtb_Scroll_This_Text(event["summary"] | "No Title");

    if (showOutlookEventTime) {
      ESP_LOGI(TAG, "🕒 Time: %s to %s\n",
             (const char*)(event["start"]["dateTime"] | event["start"]["date"]),
             (const char*)(event["end"]["dateTime"] | event["end"]["date"]));
      outlookEvent_Task_Date_1->mtb_Write_String(mtb_Format_Iso_Date((const char*)(event["start"]["dateTime"] | event["start"]["date"])));
      outlookEvent_Task_Date_2->mtb_Write_String(mtb_Format_Iso_Date((const char*)(event["start"]["dateTime"] | event["start"]["date"])));

      outlookEvent_Task_Time_1->mtb_Write_String(mtb_Format_Iso_Time((const char*)(event["start"]["dateTime"] | event["start"]["date"])));
      outlookEvent_Task_Time_2->mtb_Write_String(mtb_Format_Iso_Time((const char*)(event["start"]["dateTime"] | event["start"]["date"])));
    }

    // if (showEventStatus) {
    //   ESP_LOGI(TAG, "📍 Location: %s\n", event["location"] | "N/A");
    //   event_Task_Status_1.mtb_Write_String(event["status"] | "No Status");
    // }

    if (showOutlookEventAttendees && event.containsKey("attendees")) {
      ESP_LOGI(TAG, "👥 Attendees:\n");
      // for (JsonObject att : event["attendees"].as<JsonArray>()) {
      //   ESP_LOGI(TAG, " - %s\n", att["email"].as<const char*>());
      // }
    }

    if (showOutlookEventDescription) {
      ESP_LOGI(TAG, "📝 Description: %s\n", event["description"] | "None");
    }

    ESP_LOGI(TAG, "----\n");
  }

  http.end();
}


void fetchAllOutlookCalendarEvents(const String& accessToken) {
  HTTPClient http;
  JsonDocument doc;

  // Step 1: Fetch calendar list
  http.begin("https://www.apis.com/calendar/v3/users/me/calendarList");
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
    outlookEvent_Task_Name->mtb_Write_String(calendarName);
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

      if (showOutlookTaskTitle) {
        ESP_LOGI(TAG, "📝 Title: %s\n", task["title"] | "No Title");
      }
      if (showOutlookTaskDue && due) {
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
      if (showOutlookTaskStatus) {
        const char* stat = task["status"];
        if (strcmp(stat, "completed") == 0)
          ESP_LOGI(TAG, "✅ Completed\n");
        else
          ESP_LOGI(TAG, "❗ Pending\n");
      }
      if (showOutlookTaskNotes && task.containsKey("notes")) {
        ESP_LOGI(TAG, "📄 Notes: %s\n", task["notes"].as<const char*>());
      }
      ESP_LOGI(TAG, "----\n");
    }
  } else {
    ESP_LOGI(TAG, "Failed to get tasks: HTTP %d\n", code);
  }

  http.end();
}

  void printOutlookCalThm(void){
    mtb_Panel_Fill_Rect(0, 10, 128, 63, OUTER_SPACE);   // Fill background color2
    mtb_Panel_Fill_Rect(2, 23, 125, 61, BLACK);           // Fill clock area color.
    mtb_Panel_Draw_Line(2, 42, 126, 42, OUTER_SPACE);   // Draw inner borders1

  }

  //***************************************************************************************************
  void link_OutlookCal(JsonDocument& dCommand){
  }

  void get_OutlookCal_Refresh_Token(JsonDocument& dCommand){
    const char *refreshToken = dCommand["refreshToken"];
    strcpy(userOutlookCal.refreshToken, refreshToken);
    mtb_Write_Nvs_Struct("outlookCalData", &userOutlookCal, sizeof(OutlookCal_Data_t));
    do_beep(CLICK_BEEP);
    statusBarNotif.mtb_Scroll_This_Text("OUTLOOK CALENDAR LINK UPDATED. YOU MAY CLOSE THE BROWSER", GREEN_LIZARD);
  }



  void show_OutlookCal_Events(JsonDocument& dCommand){
    uint8_t setCycle = dCommand["showEvents"];
    mtb_Write_Nvs_Struct("outlookCalData", &userOutlookCal, sizeof(OutlookCal_Data_t));
  }
  
  void show_OutlookCal_Tasks(JsonDocument& dCommand){
    uint8_t setCycle = dCommand["showTasks"];
    mtb_Write_Nvs_Struct("outlookCalData", &userOutlookCal, sizeof(OutlookCal_Data_t));
  }

  void show_OutlookCal_Holidays(JsonDocument& dCommand){
    uint8_t setCycle = dCommand["showHoliday"];
    mtb_Write_Nvs_Struct("outlookCalData", &userOutlookCal, sizeof(OutlookCal_Data_t));
  }
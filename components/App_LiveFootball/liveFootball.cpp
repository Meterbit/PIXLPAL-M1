#include <Arduino.h>
#include "mtbGithubStorage.h"
#include "scrollMsgs.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "nvsMem.h"
#include "liveFootball.h"
#include "mtbApps.h"
#include "liveMatchJson.h"
#include "ledPanel.h"
#include "beep.h"

// Your API-Football subscription key from RapidAPI
const char *API_KEY = "a2e0674bdfb9d68a5e001e391bcb5160";

// Base URL for API endpoints
const char* BASE_URL = "https://v3.football.api-sports.io";

int16_t liveFootballDispChangeIntv = 0;  // This variable controls the time it takes for what is being displayed on-screen to change

LiveFootball_Data_t liveFootballData;

EXT_RAM_BSS_ATTR SemaphoreHandle_t changeDispMatch_Sem = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t liveFootball_Task_H = NULL;
void liveFootball_App_Task(void *);
// supporting functions


void onTimerCallback(TimerHandle_t xTimer);
void inboundMatchTimer(time_t targetTimestamp);
String processJsonCommand(uint8_t type = 0, uint16_t leagueId = 39);

bool fetchLiveMatchTeamLogos(SpiRamJsonDocument&, size_t matchIndex = 0);
bool fetchFixturesMatchTeamLogos(SpiRamJsonDocument&, size_t matchIndex = 0);

void wipePrevFixturesLogos(void);
void drawLiveMatchesBackground(void);
void drawMatchFixturesBackground(void);
void drawStandingsBackground(void);

void processLiveMatches(SpiRamJsonDocument& doc, void*);
void processFituresMatches(SpiRamJsonDocument& doc, void*);
void processStandingsTable(SpiRamJsonDocument& doc, void*);

// Define a function pointer type
typedef void (*LiveFootballEndpointFn_ptr)(SpiRamJsonDocument&, void*);
typedef void (*LiveFootballBackgroundFn_ptr)(void);

LiveFootballEndpointFn_ptr liveFootballPtr;
LiveFootballBackgroundFn_ptr liveFootballBackgroundPtr;

// button and encoder functions
void changeFootballTeams(button_event_t button_Data);

// bluetooth functions
void selectFBL_Leagues(DynamicJsonDocument&);
void setDisplayFBL_League(DynamicJsonDocument &);
void saveFBL_Leagues(DynamicJsonDocument &);
void showFBL_Fix_Stnd(DynamicJsonDocument &);
void setFBL_Token(DynamicJsonDocument &);


CentreText_t* scoreLineLive;
CentreText_t* elapsedTimeLive;
ScrollText_t* moreDataScroll;

CentreText_t* statsTitle;

CentreText_t* serialFixtures;
CentreText_t* dateFixtures;
CentreText_t* timeFixtures;
CentreText_t* vsFixtures;

FixedText_t* rankStandings1;
FixedText_t* teamStandings1;
FixedText_t* pointsStandings1;

FixedText_t* rankStandings2;
FixedText_t* teamStandings2;
FixedText_t* pointsStandings2;

FixedText_t* rankStandings3;
FixedText_t* teamStandings3;
FixedText_t* pointsStandings3;

FixedText_t* rankStandings4;
FixedText_t* teamStandings4;
FixedText_t* pointsStandings4;

EXT_RAM_BSS_ATTR Applications_StatusBar *liveFootbalScores_App = new Applications_StatusBar(liveFootball_App_Task, &liveFootball_Task_H, "Live Football", 12288);



void liveFootball_App_Task(void *dApplication){
    Applications *thisApp = (Applications *)dApplication;
    thisApp->app_EncoderFn_ptr = brightnessControl;
    thisApp->app_ButtonFn_ptr = changeFootballTeams;
    ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(selectFBL_Leagues, setDisplayFBL_League, saveFBL_Leagues, showFBL_Fix_Stnd, setFBL_Token);
    appsInitialization(thisApp, statusBarClock_Sv);
    //************************************************************************************ */

    if(changeDispMatch_Sem == NULL) changeDispMatch_Sem = xSemaphoreCreateBinary();

    scoreLineLive = new CentreText_t(63, 29, Terminal10x16, WHITE);
    elapsedTimeLive = new CentreText_t(63, 46, Terminal6x8, CYAN);
    moreDataScroll = new ScrollText_t(0, 55, 128, WHITE, 10, 1, Terminal6x8);

    statsTitle = new CentreText_t(63, 15, Terminal6x8, WHITE, TURQUOISE);

    serialFixtures = new CentreText_t(107, 27, Terminal6x8, WHITE);
    dateFixtures = new CentreText_t(107, 37, Terminal4x6, GREEN_LIZARD);
    timeFixtures = new CentreText_t(107, 47, Terminal6x8, CYAN);
    vsFixtures = new CentreText_t(43, 37, Terminal6x8, WHITE);

    rankStandings1 = new FixedText_t(2, 23, Terminal6x8, GREEN);
    teamStandings1 = new FixedText_t(17, 23, Terminal6x8, GREEN_LIZARD);
    pointsStandings1 = new FixedText_t(115, 23, Terminal6x8, WHITE);

    rankStandings2 = new FixedText_t(2, 34, Terminal6x8, GREEN);
    teamStandings2 = new FixedText_t(17, 34, Terminal6x8, GREEN_LIZARD);
    pointsStandings2 = new FixedText_t(115, 34, Terminal6x8, WHITE);

    rankStandings3 = new FixedText_t(2, 45, Terminal6x8, GREEN);
    teamStandings3 = new FixedText_t(17, 45, Terminal6x8, GREEN_LIZARD);
    pointsStandings3 = new FixedText_t(115, 45, Terminal6x8, WHITE);

    rankStandings4 = new FixedText_t(2, 56, Terminal6x8, GREEN);
    teamStandings4 = new FixedText_t(17, 56, Terminal6x8, GREEN_LIZARD);
    pointsStandings4 = new FixedText_t(115, 56, Terminal6x8, WHITE);

    // Optional: Replace with your certificate authority if needed
    WiFiClientSecure client;
    const size_t CAPACITY = 150 * 1024; // ~150 KB, adjust to fit your JSON
    SpiRamJsonDocument doc(CAPACITY);
        read_struct_from_nvs("apiFutBall", &liveFootballData, sizeof(LiveFootball_Data_t));
        liveFootballData.endpointType = LIVE_MATCHES_ENDPOINT;
        liveFootballPtr = processLiveMatches;
        liveFootballBackgroundPtr = drawLiveMatchesBackground;

        // Set client to insecure for now (you can secure with fingerprint or CA cert)
        client.setInsecure();

    while (THIS_APP_IS_ACTIVE == pdTRUE){

        while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);

        liveFootballDispChangeIntv = 0;
        liveFootballBackgroundPtr();

        while (THIS_APP_IS_ACTIVE == pdTRUE && liveFootballDispChangeIntv <= 0){
        String fullUrl = BASE_URL + processJsonCommand(liveFootballData.endpointType, liveFootballData.leagueID);
        printf("The Full URL is: %s\n", fullUrl.c_str());
        // Optional: stop here or repeat after a longer delay
        HTTPClient http;
        http.begin(client, fullUrl);
        http.addHeader("x-apisports-key", API_KEY);
        int httpResponseCode = http.GET();

        if (httpResponseCode == 200){
          String payload = http.getString();
          DeserializationError error = deserializeJson(doc, payload);
          //printf("\n\nThe payload is: %s\n\n", payload.c_str());

          if (!error) {
            if(doc["response"].isNull() || doc["response"].size() == 0){
              liveFootballDispChangeIntv = 300; // Set a longer interval to wait for the next fetch
              read_struct_from_nvs("apiFutBall", &liveFootballData, sizeof(LiveFootball_Data_t));
              moreDataScroll->scroll_This_Text("NO DATA FOR THIS SELECTION. TRY A DIFFERENT LEAGUE", GREEN);
              
              switch(liveFootballData.endpointType){
              case FIXTURES_ENDPOINT:
                  liveFootballPtr = processFituresMatches;
                  liveFootballBackgroundPtr = drawMatchFixturesBackground;
              break;

              case STANDINGS_ENDPOINT:
                  liveFootballPtr = processStandingsTable;
                  liveFootballBackgroundPtr = drawStandingsBackground;
              break;

              default:
                  liveFootballData.endpointType = FIXTURES_ENDPOINT; // Default to fixtures
                  liveFootballPtr = processFituresMatches;
                  liveFootballBackgroundPtr = drawMatchFixturesBackground;
              break;
              }
            } else {
              liveFootballPtr(doc, thisApp);
            }
          } else {
            printf("JSON parse error: %s\n", error.c_str());
          }
          delay(5000); // Wait before next fetch
        } else {
          printf("HTTP GET failed with code: %d\n", httpResponseCode);
          delay(5000); // Wait before retrying
        }
        http.end();
        }
    }

    delete scoreLineLive;
    delete elapsedTimeLive;
    delete moreDataScroll;

    delete statsTitle;
    delete serialFixtures;
    delete dateFixtures;
    delete timeFixtures;
    delete vsFixtures;

    delete rankStandings1;
    delete teamStandings1;
    delete pointsStandings1;

    delete rankStandings2;
    delete teamStandings2;
    delete pointsStandings2;

    delete rankStandings3;
    delete teamStandings3;
    delete pointsStandings3;

    delete rankStandings4;
    delete teamStandings4;
    delete pointsStandings4;

    kill_This_App(thisApp);
}


  
void processLiveMatches(SpiRamJsonDocument& doc, void* dApplication){
      Applications *thisApp = (Applications *)dApplication;
      JsonArray matches = doc["response"].as<JsonArray>();

      if (matches.isNull() || matches.size() == 0) {
          liveFootballData.endpointType = FIXTURES_ENDPOINT;
          liveFootballPtr = processFituresMatches;
          liveFootballBackgroundPtr = drawMatchFixturesBackground;
          liveFootballDispChangeIntv = 150;
          return;
      }

      int matchIndex = 0;

      for (JsonObject match : matches) {
        liveFootballDispChangeIntv = 150;
        fetchLiveMatchTeamLogos(doc, matchIndex++); // Draw logos for the first match
        
        // Extract data
        String homeTeam = match["teams"]["home"]["name"].as<String>();
        String awayTeam = match["teams"]["away"]["name"].as<String>();

        // Navigate to fixture.status
        JsonObject status = match["fixture"]["status"];

        const char* shortStatus = status["short"] | "";
        int elapsed = status["elapsed"] | 0;
        int extra = status["extra"] | 0;  // Will be 0 if null

        // Construct output string
        String matchStatus = String(shortStatus) + "  " + String(elapsed) + "'";
        if (status["extra"] != nullptr) {
          matchStatus += "+" + String(extra);
        }

        int homeGoals = match["goals"]["home"] | 0;
        int awayGoals = match["goals"]["away"] | 0;
        String leagueName = match["league"]["name"].as<String>();
        String round = match["league"]["round"].as<String>();
        String venue = match["fixture"]["venue"]["name"].as<String>();

        String scoreText = String(homeGoals) + "-" + String(awayGoals);
        scoreLineLive->writeColoredString(scoreText.c_str(), WHITE);
        elapsedTimeLive->writeColoredString(matchStatus.c_str(), CYAN);

        String dLeague = leagueName + " (" + String(round) + ")";
        moreDataScroll->scroll_This_Text(dLeague, CYAN);
        String dTeams = homeTeam + " vs " + awayTeam;
        moreDataScroll->scroll_This_Text(dTeams, GREEN);
        //moreDataScroll->scroll_This_Text(venue, BROWN);

        JsonArray events = match["events"].as<JsonArray>();
        if (events.size() > 0) {
          for (JsonObject ev : events) {
            int eTime = ev["time"]["elapsed"] | 0;
            String team = ev["team"]["name"] | "Unknown";
            String player = ev["player"]["name"] | "Unknown";
            String type = ev["type"] | "Unknown";
            String detail = ev["detail"] | "Unknown";

            String eventsText = String(eTime) + "' - " + team + ": " + player + " - " + type + " (" + detail + ")";
            moreDataScroll->scroll_This_Text(eventsText, YELLOW);
          }
        } else {
          printf("  No events recorded.\n");
        }

      while(liveFootballDispChangeIntv-->0 && xSemaphoreTake(changeDispMatch_Sem, 0) != pdTRUE && THIS_APP_IS_ACTIVE == pdTRUE) delay(100);
      if(liveFootballDispChangeIntv > 0) break;
      }
}


void processFituresMatches(SpiRamJsonDocument& doc, void* dApplication){
  Applications *thisApp = (Applications *)dApplication;
  JsonArray matches = doc["response"].as<JsonArray>();
  int results = doc["results"] | 0;
  uint8_t noOfShowCycles = 5;

while(noOfShowCycles-->0){
    int matchIndex = 0;
  for (JsonObject match : matches) {
    liveFootballDispChangeIntv = 30;

    time_t time = match["fixture"]["timestamp"];
    String leagueName = match["league"]["name"];

    fetchFixturesMatchTeamLogos(doc, matchIndex); // Draw logos for the first match

    String status = match["fixture"]["status"]["short"] | "N/A";

    if (matchIndex == 0 && status != "NS" && status != "TBD"){
      //printf("Processing Live match %d\n", matchIndex);
      liveFootballData.endpointType = LIVE_MATCHES_ENDPOINT;
      liveFootballPtr = processLiveMatches;
      liveFootballBackgroundPtr = drawLiveMatchesBackground;
      do_beep(BEEP_1);
      return;
    }

    String venue = match["fixture"]["venue"]["name"] | "Unknown Venue";
    String homeTeam = match["teams"]["home"]["name"];
    String awayTeam = match["teams"]["away"]["name"];
    String round = match["league"]["round"];

    statsTitle->writeColoredString(leagueName, BLACK, TEAL);

    serialFixtures->writeColoredString(String(++matchIndex) + "/" + String(results), BLUE_GRAY);
    dateFixtures->writeColoredString(formatDateFromTimestamp(time), GREEN_LIZARD);
    timeFixtures->writeColoredString(formatTimeFromTimestamp(time), CYAN);
    vsFixtures->writeColoredString("vs", ASH_GRAY);

    String dTeams = homeTeam + " vs " + awayTeam;

    moreDataScroll->scroll_This_Text(round, CYAN);
    moreDataScroll->scroll_This_Text(dTeams, GREEN);
    moreDataScroll->scroll_This_Text(venue, PINK);

    while(liveFootballDispChangeIntv-->0 && xSemaphoreTake(changeDispMatch_Sem, 0) != pdTRUE && THIS_APP_IS_ACTIVE == pdTRUE) delay(100);
    if(liveFootballDispChangeIntv > 0) break;
  }
  if(liveFootballDispChangeIntv > 0) break;
}
  moreDataScroll->scroll_Active(STOP_SCROLL);
}



void processStandingsTable(SpiRamJsonDocument& doc, void* dApplication){
      Applications *thisApp = (Applications *)dApplication;
      JsonArray standings = doc["response"][0]["league"]["standings"];
      //String leagueName = doc["response"][0]["league"]["name"] | "Unknown League";

      uint8_t noOfShowCycles = 5;
      while(noOfShowCycles-->0){
              int standingIndex = 0;
        for (JsonArray group : standings) {
          String groupName = group[0]["group"];
        //titleStandings->writeColoredString(leagueName, BLACK);
          statsTitle->writeColoredString(groupName, BLACK, TURTLE_GREEN);
          for (JsonObject team : group) {
            liveFootballDispChangeIntv = 75;

            if ((standingIndex % 4) == 0) {
              rankStandings1->writeColoredString(String(team["rank"].as<int>()).c_str(), YELLOW_CRAYOLA);
              teamStandings1->writeColoredString(team["team"]["name"].as<const char*>(), GREEN_LIZARD);
              pointsStandings1->writeColoredString(String(team["points"].as<int>()), WHITE);
            } else if ((standingIndex % 4) == 1) {
              rankStandings2->writeColoredString(String(team["rank"].as<int>()).c_str(), YELLOW_CRAYOLA);
              teamStandings2->writeColoredString(team["team"]["name"].as<const char*>(), GREEN_LIZARD);
              pointsStandings2->writeColoredString(String(team["points"].as<int>()), WHITE);
            } else if ((standingIndex % 4) == 2) {
              rankStandings3->writeColoredString(String(team["rank"].as<int>()).c_str(), YELLOW_CRAYOLA);
              teamStandings3->writeColoredString(team["team"]["name"].as<const char*>(), GREEN_LIZARD);
              pointsStandings3->writeColoredString(String(team["points"].as<int>()), WHITE);
            } else if ((standingIndex % 4) == 3) {
              rankStandings4->writeColoredString(String(team["rank"].as<int>()).c_str(), YELLOW_CRAYOLA);
              teamStandings4->writeColoredString(team["team"]["name"].as<const char*>(), GREEN_LIZARD);
              pointsStandings4->writeColoredString(String(team["points"].as<int>()), WHITE);
            }

            // Delay after every 4 teams
            if (++standingIndex % 4 == 0) {
              for (; liveFootballDispChangeIntv--> 0;) {
                if (xSemaphoreTake(changeDispMatch_Sem, 0) == pdTRUE || THIS_APP_IS_ACTIVE == false) {
                  return;
                }
                delay(100);
              }
            }
          }
        }
      }
        //printf("\n=======================\n");
}



int getCurrentYear() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  return timeinfo.tm_year + 1900;
}



String processJsonCommand(uint8_t type, uint16_t leagueId) {
  time_t now = time(nullptr);
  String path;
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  int season = getCurrentYear();

  switch(type){
    case 0: {
    int nextCount = 10; 
      path = "/fixtures?league=" + String(leagueId) +
            "&season=" + String(season) +
            "&next=" + String(nextCount);
      break;
      }
    case 1: 
      path = "/standings?league=" + String(leagueId) + "&season=" + String(season);
    break;

    case 2: 
      path = "/fixtures?live=all&league=" + String(leagueId);
    break;

    default: 
      printf("Invalid type or missing parameters.\n");
      path = "/fixtures?live=all&league=" + String(leagueId);
    break; 
  }

  // fetchAndPrintJson(path);
  return path;
}



bool fetchLiveMatchTeamLogos(SpiRamJsonDocument& doc, size_t matchIndex) {

    PNG_OnlineImage_t pnglogoBatch[2];
    SVG_OnlineImage_t svgLogoBatch[2];

    JsonArray matches = doc["response"].as<JsonArray>();
    if (matchIndex >= matches.size()) {
        printf("Invalid match index: %d\n", matchIndex);
        return false;
    }

    JsonObject match = matches[matchIndex];
    const char* homeLogo = match["teams"]["home"]["logo"];
    const char* awayLogo = match["teams"]["away"]["logo"];


    String homeName = match["teams"]["home"]["name"];
    String awayName = match["teams"]["away"]["name"];

    String countryHomeLink = getFlagUrlByCountryName(homeName, "flag_1x1");
    String countryAwayLink = getFlagUrlByCountryName(awayName, "flag_1x1");

    if (countryHomeLink.length() && countryAwayLink.length()) {

      strncpy(svgLogoBatch[0].imageLink, countryHomeLink.c_str(), sizeof(svgLogoBatch[0].imageLink));
      svgLogoBatch[0].xAxis = 4;
      svgLogoBatch[0].yAxis = 12;
      svgLogoBatch[0].scale = 2;

      strncpy(svgLogoBatch[1].imageLink, countryAwayLink.c_str(), sizeof(svgLogoBatch[1].imageLink));
      svgLogoBatch[1].xAxis = 86;
      svgLogoBatch[1].yAxis = 12;
      svgLogoBatch[1].scale = 2;

      downloadMultipleOnlineSVGs(svgLogoBatch, 2);
      drawMultipleSVGs(2);
      return true;
    } else {
          printf("Home and Away logos are both PNGs.\n");

          strncpy(pnglogoBatch[0].imageLink, homeLogo, sizeof(pnglogoBatch[0].imageLink));
          pnglogoBatch[0].xAxis = 6;
          pnglogoBatch[0].yAxis = 15;
          pnglogoBatch[0].scale = 5;

          strncpy(pnglogoBatch[1].imageLink, awayLogo, sizeof(pnglogoBatch[1].imageLink));
          pnglogoBatch[1].xAxis = 88;
          pnglogoBatch[1].yAxis = 15;
          pnglogoBatch[1].scale = 5;

          downloadMultipleOnlinePNGs(pnglogoBatch, 2);
          if(matchIndex > 0) drawMultiplePNGs(2, wipePrevFixturesLogos);
          else drawMultiplePNGs(2);
          return true;
    }

    statusBarNotif.scroll_This_Text("Invalid logo format for match index: " + String(matchIndex), RED);

  return false;
}



bool fetchFixturesMatchTeamLogos(SpiRamJsonDocument& doc, size_t matchIndex) {

    PNG_OnlineImage_t pnglogoBatch[2];
    SVG_OnlineImage_t svgLogoBatch[2];

    JsonArray matches = doc["response"].as<JsonArray>();
    if (matchIndex >= matches.size()) {
        printf("Invalid match index: %d\n", matchIndex);
        return false;
    }

    JsonObject match = matches[matchIndex];
    const char* homeLogo = match["teams"]["home"]["logo"];
    const char* awayLogo = match["teams"]["away"]["logo"];

    String homeName = match["teams"]["home"]["name"];
    String awayName = match["teams"]["away"]["name"];

    String countryHomeLink = getFlagUrlByCountryName(homeName, "flag_1x1");
    String countryAwayLink = getFlagUrlByCountryName(awayName, "flag_1x1");


    if (countryHomeLink.length() && countryAwayLink.length()) {

        strncpy(svgLogoBatch[0].imageLink, countryHomeLink.c_str(), sizeof(svgLogoBatch[0].imageLink));
        svgLogoBatch[0].xAxis = 1;
        svgLogoBatch[0].yAxis = 22;
        svgLogoBatch[0].scale = 2;

        strncpy(svgLogoBatch[1].imageLink, countryAwayLink.c_str(), sizeof(svgLogoBatch[1].imageLink));
        svgLogoBatch[1].xAxis = 52;
        svgLogoBatch[1].yAxis = 22;
        svgLogoBatch[1].scale = 2;

        downloadMultipleOnlineSVGs(svgLogoBatch, 2);
        drawMultipleSVGs(2);
        return true;
    } else {

        strncpy(pnglogoBatch[0].imageLink, homeLogo, sizeof(pnglogoBatch[0].imageLink));
        pnglogoBatch[0].xAxis = 2;
        pnglogoBatch[0].yAxis = 22;
        pnglogoBatch[0].scale = 5;

        strncpy(pnglogoBatch[1].imageLink, awayLogo, sizeof(pnglogoBatch[1].imageLink));
        pnglogoBatch[1].xAxis = 53;
        pnglogoBatch[1].yAxis = 22;
        pnglogoBatch[1].scale = 5;

        downloadMultipleOnlinePNGs(pnglogoBatch, 2);

        if(matchIndex > 0) drawMultiplePNGs(2, wipePrevFixturesLogos);
        else drawMultiplePNGs(2);
        return true;
    } 
    
    statusBarNotif.scroll_This_Text("Invalid logo format for match index: " + String(matchIndex), RED);

    return false;
}


void changeFootballTeams(button_event_t button_Data){
    switch (button_Data.type)
    {
    case BUTTON_RELEASED:
        break;

    case BUTTON_PRESSED:
        break;

    case BUTTON_PRESSED_LONG:
        break;

    case BUTTON_CLICKED:
        // printf("Button Clicked: %d Times\n",button_Data.count);
        switch (button_Data.count)
        {
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



void wipePrevFixturesLogos(void){
  dma_display->fillRect(16, 22, 21, 30, BLACK); // Fill background color2
  dma_display->fillRect(67, 22, 21, 30, BLACK); // Fill background color2
  dma_display->fillRect(36, 22, 6, 11, BLACK);   // Fill background color2
  dma_display->fillRect(36, 41, 6, 11, BLACK);   // Fill background color2
}

void drawLiveMatchesBackground(void){
    dma_display->fillRect(0, 10, 128, 54, BLACK);
}

void drawMatchFixturesBackground(void){
    dma_display->fillRect(0, 10, 128, 54, BLACK);
    dma_display->fillRect(0, 10, 128, 10, TEAL);   // Fill background color2
    dma_display->drawFastVLine(88, 22, 32, dma_display->color565(35, 35, 35));
}

void drawStandingsBackground(void){
    dma_display->fillRect(0, 10, 128, 54, BLACK);   
    dma_display->fillRect(0, 10, 128, 10, TURTLE_GREEN);
    uint16_t teamDividerColor = dma_display->color565(35, 35, 35);
    dma_display->drawFastHLine(0, 31, 128, teamDividerColor);
    dma_display->drawFastHLine(0, 42, 128, teamDividerColor);
    dma_display->drawFastHLine(0, 53, 128, teamDividerColor);
}


//************************************************************************************************* */
//***************************************************************************************************

//******** BLUETOOTH COMMAND 0 **********************/
void selectFBL_Leagues(DynamicJsonDocument& dCommand){
  uint8_t cmd = dCommand["app_command"];
  ble_Application_Command_Respond_Success(liveFootbalAppRoute, cmd, pdPASS);
}

//******** BLUETOOTH COMMAND 1 **********************/
void setDisplayFBL_League(DynamicJsonDocument &dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String leagueName = dCommand["leagueName"];
    uint16_t leagueId = dCommand["leagueID"];

    liveFootballData.leagueID = leagueId;
    xSemaphoreGive(changeDispMatch_Sem);
    write_struct_to_nvs("apiFutBall", &liveFootballData, sizeof(LiveFootball_Data_t));
    ble_Application_Command_Respond_Success(liveFootbalAppRoute, cmdNumber, pdPASS);
}
//******** BLUETOOTH COMMAND 2 **********************/
void saveFBL_Leagues(DynamicJsonDocument &dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    ble_Application_Command_Respond_Success(liveFootbalAppRoute, cmdNumber, pdPASS);
}

//******** BLUETOOTH COMMAND 3 **********************/
void showFBL_Fix_Stnd(DynamicJsonDocument &dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    uint8_t showFix_Stnd = dCommand["value"];

    printf("The command clicked is: %d\n", cmdNumber);

    if(showFix_Stnd == 0){
        liveFootballData.endpointType = FIXTURES_ENDPOINT;
        liveFootballPtr = processFituresMatches;
        liveFootballBackgroundPtr = drawMatchFixturesBackground;
    } else {
        liveFootballData.endpointType = STANDINGS_ENDPOINT;
        liveFootballPtr = processStandingsTable;
        liveFootballBackgroundPtr = drawStandingsBackground;
    }
    xSemaphoreGive(changeDispMatch_Sem); 
    write_struct_to_nvs("apiFutBall", &liveFootballData, sizeof(LiveFootball_Data_t));
    ble_Application_Command_Respond_Success(liveFootbalAppRoute, cmdNumber, pdPASS);
}
//******** BLUETOOTH COMMAND 4 **********************/
void setFBL_Token(DynamicJsonDocument &dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String liveFootballToken = dCommand["api_key"];
    //strcpy(liveFootballData.userAPI_Token, liveFootballToken.c_str());

    printf("The command clicked is: %d\n", cmdNumber);

    xSemaphoreGive(changeDispMatch_Sem); 
    write_struct_to_nvs("apiFutBall", &liveFootballData, sizeof(LiveFootball_Data_t));
    ble_Application_Command_Respond_Success(liveFootbalAppRoute, cmdNumber, pdPASS);
}
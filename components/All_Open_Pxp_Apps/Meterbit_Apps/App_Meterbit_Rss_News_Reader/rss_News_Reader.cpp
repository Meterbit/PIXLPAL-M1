#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include "mtb_engine.h"
#include "mtb_nvs.h"
#include "mtb_text_scroll.h"
#include "tinyxml2.h"

static const char TAG[] = "PXP_RSS_NEWS";

using namespace tinyxml2;

#define RSS_REFRESH_INTERVAL_MINUTES 3

EXT_RAM_BSS_ATTR TaskHandle_t rssNewsApp_Task_H = NULL;
void rssNewsApp_Task(void *dApplication);

EXT_RAM_BSS_ATTR Mtb_ScrollText_t *rssScroller = new Mtb_ScrollText_t(2, 52, 124, Terminal6x8, WHITE, 20, 0xFFFF);  // REVISIT -> Make Global declaration dynamic. Move definitions to PSRAM
EXT_RAM_BSS_ATTR Mtb_ScrollText_t *rssErrorMsg = new Mtb_ScrollText_t(2, 42, 124, Terminal6x8, ORANGE_RED, 20, 3);  // REVISIT -> Make Global declaration dynamic. Move definitions to PSRAM


EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *rssNewsApp = new Mtb_Applications_StatusBar(rssNewsApp_Task, &rssNewsApp_Task_H, "RSS News", 8192, pdTRUE);

const char* bbc_url = "https://feeds.bbci.co.uk/news/rss.xml";
const char* cnn_url = "http://rss.cnn.com/rss/edition.rss";
//const char* reuters_url = "http://feeds.reuters.com/reuters/topNews";

struct RssSettings_t {
  bool enable_bbc;
  bool enable_cnn;
  bool enable_reuters;
};

RssSettings_t rssSettings = {true, false, false};

void fetchAndDisplayHeadlines(Mtb_Applications *thisApp);
void updateSourceSelection(JsonDocument&);

void rssNewsApp_Task(void* dApplication) {
  Mtb_Applications *thisApp = (Mtb_Applications *) dApplication;
  thisApp->mtb_App_Set_Ble_Comm_Sv_Fns(updateSourceSelection);

  thisApp->mtb_App_Init(mtb_Status_Bar_Clock_Sv);
  mtb_Read_Nvs_Struct("rssSettings", &rssSettings, sizeof(RssSettings_t));

  // Wait for internet connection before proceeding
  while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);

  while (MTB_APP_IS_ACTIVE == pdTRUE) {
    fetchAndDisplayHeadlines(thisApp);
    for (int i = 0; i < RSS_REFRESH_INTERVAL_MINUTES * 60 && MTB_APP_IS_ACTIVE == pdTRUE; ++i) {
      delay(1000);
    }
  }
  delete rssScroller;
  delete rssErrorMsg;

  mtb_Delete_This_App(thisApp);
}

void updateSourceSelection(JsonDocument &doc) {
  rssSettings.enable_bbc = doc["bbc"];
  rssSettings.enable_cnn = doc["cnn"];
  rssSettings.enable_reuters = doc["reuters"];

  mtb_Write_Nvs_Struct("rssSettings", &rssSettings, sizeof(RssSettings_t));
}

void parseHeadlinesWithTinyXML2(const String& xml, std::vector<String>& headlines, int maxCount) {
  XMLDocument doc;
  if (doc.Parse(xml.c_str()) != XML_SUCCESS) return;

  XMLElement* root = doc.FirstChildElement("rss");
  if (!root) return;

  XMLElement* channel = root->FirstChildElement("channel");
  if (!channel) return;

  XMLElement* item = channel->FirstChildElement("item");
  while (item && headlines.size() < maxCount) {
    const char* title = item->FirstChildElement("title")->GetText();
    if (title) headlines.push_back(String(title));
    item = item->NextSiblingElement("item");
  }
}

bool fetchFeed(const char* url, std::vector<String>& headlines) {
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    parseHeadlinesWithTinyXML2(payload, headlines, 5);
    http.end();
    return true;
  } else {
    http.end();
    return false;
  }
}

void fetchAndDisplayHeadlines(Mtb_Applications *thisApp) {
  std::vector<String> headlines;
  bool bbcOk = !rssSettings.enable_bbc || fetchFeed(bbc_url, headlines);
  bool cnnOk = !rssSettings.enable_cnn || fetchFeed(cnn_url, headlines);
  //bool reutersOk = !rssSettings.enable_reuters || fetchFeed(reuters_url, headlines);

  if (!bbcOk || !cnnOk /*|| !reutersOk*/ ) {
    String errorMsg = "Feed error: ";
    if (!bbcOk) errorMsg += "BBC ";
    if (!cnnOk) errorMsg += "CNN ";
    //if (!reutersOk) errorMsg += "Reuters ";
    rssErrorMsg->mtb_Scroll_This_Text(errorMsg.c_str());
  } else {
    rssErrorMsg->mtb_Scroll_Active(STOP_SCROLL);
  }

  for (auto& headline : headlines) {
    if (MTB_APP_IS_ACTIVE != pdTRUE) break;
    rssScroller->mtb_Scroll_This_Text(headline.c_str());
    ESP_LOGI(TAG, "RSS news content is: %s\n", headline.c_str());
    delay(8000);
  }
}

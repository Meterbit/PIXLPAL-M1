#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "SpotifyPlayback.h"
#include "SpotifyAuth.h"

static const char TAG[] = "SPOTIFY_PLAYBACK";

const char* api_host = "https://api.spotify.com";

void displayNowPlaying(String track, String artist) {
  ESP_LOGI(TAG, "Now Playing: %s\n", track.c_str());
  ESP_LOGI(TAG, "by ");
  ESP_LOGI(TAG, "%s", artist.c_str());
  ESP_LOGI(TAG, "\n");
}

void getNowPlaying() {
  HTTPClient http;
  String url = String(api_host) + "/v1/me/player";

  http.begin(url);
  http.addHeader("Authorization", "Bearer " + access_token);
  http.addHeader("Connection", "close");

  int httpCode = http.GET();
  ESP_LOGI(TAG, "HTTP Status: %d\n", httpCode);

    if (httpCode == 204) {
    ESP_LOGI(TAG, "No active playback — start a song in the Spotify app.\n");
    http.end();
    return;
  }

  if (httpCode > 0) {
    String body = http.getString();
    ESP_LOGI(TAG, "Raw response:\n%s\n", body.c_str());

    JsonDocument doc;  // allow large metadata
    DeserializationError err = deserializeJson(doc, body);

    if (err) {
      ESP_LOGI(TAG, "JSON parse error: %s\n", err.c_str());
    } else if (doc.containsKey("item") && !doc["item"].isNull()) {
      String track = doc["item"]["name"].as<String>();
      String artist = doc["item"]["artists"][0]["name"].as<String>();
      displayNowPlaying(track, artist);
    } else {
      ESP_LOGI(TAG, "No track info: 'item' is missing or null.\n");
    }
  } else {
    ESP_LOGI(TAG, "HTTP GET failed: %d\n", httpCode);
  }

  http.end();
}

void sendPlaybackCommand(const String& command, const String& method) {
  HTTPClient http;
  String url = String(api_host) + "/v1/me/player/" + command;

  http.begin(url);
  http.addHeader("Authorization", "Bearer " + access_token);
  http.addHeader("Content-Length", "0");
  http.addHeader("Connection", "close");

  int httpCode = (method == "PUT")
                   ? http.sendRequest("PUT")
                   : http.POST("");  // send empty body

  ESP_LOGI(TAG, "Sent command: %s, HTTP status: %d\n", command.c_str(), httpCode);
  http.end();
}

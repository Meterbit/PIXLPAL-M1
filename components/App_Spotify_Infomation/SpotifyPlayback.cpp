#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "SpotifyPlayback.h"
#include "SpotifyAuth.h"

const char* api_host = "https://api.spotify.com";

void displayNowPlaying(String track, String artist) {
  printf("Now Playing: %s\n", track.c_str());
  printf("by ");
  printf(artist.c_str());
  printf("\n");
}

void getNowPlaying() {
  HTTPClient http;
  String url = String(api_host) + "/v1/me/player";

  http.begin(url);
  http.addHeader("Authorization", "Bearer " + access_token);
  http.addHeader("Connection", "close");

  int httpCode = http.GET();
  printf("HTTP Status: %d\n", httpCode);

    if (httpCode == 204) {
    printf("No active playback — start a song in the Spotify app.\n");
    http.end();
    return;
  }

  if (httpCode > 0) {
    String body = http.getString();
    printf("Raw response:\n%s\n", body.c_str());

    DynamicJsonDocument doc(8192);  // allow large metadata
    DeserializationError err = deserializeJson(doc, body);

    if (err) {
      printf("JSON parse error: %s\n", err.c_str());
    } else if (doc.containsKey("item") && !doc["item"].isNull()) {
      String track = doc["item"]["name"].as<String>();
      String artist = doc["item"]["artists"][0]["name"].as<String>();
      displayNowPlaying(track, artist);
    } else {
      printf("No track info: 'item' is missing or null.\n");
    }
  } else {
    printf("HTTP GET failed: %d\n", httpCode);
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

  printf("Sent command: %s, HTTP status: %d\n", command.c_str(), httpCode);
  http.end();
}

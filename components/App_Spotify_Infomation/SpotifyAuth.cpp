// ===== File: SpotifyAuth.cpp =====
#include <HTTPClient.h>
#include "SpotifyAuth.h"
#include "mtbSpotifyInfo.h"
#include "nvsMem.h"

const char* token_host = "accounts.spotify.com";
String access_token;

bool getAccessToken(const char* client_id, const char* refresh_token) {
  HTTPClient http;

  http.begin("https://accounts.spotify.com/api/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String postData = "grant_type=refresh_token&refresh_token=" + String(refresh_token) + "&client_id=" + String(client_id);

  int httpCode = http.POST(postData);
  String body = http.getString(); // ✅ already just JSON
  http.end();

  if (httpCode <= 0) {
    printf("HTTP POST failed: %d\n", httpCode);
    return false;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    printf("Failed to parse JSON: %s\n", err.c_str());
    return false;
  }

  if (doc.containsKey("access_token")) {
    access_token = doc["access_token"].as<String>();

    if (doc.containsKey("refresh_token")) {
      String newRefreshToken = doc["refresh_token"].as<String>();
      strcpy(userSpotify.refreshToken, newRefreshToken.c_str());
      write_struct_to_nvs("spotifyData", &userSpotify, sizeof(Spotify_Data_t));
      printf("New refresh token saved.\n");
    }

    printf("Access token obtained.\n");
    return true;
  }

  printf("Failed to obtain token.\nBody: %s\n", body.c_str());
  return false;
}

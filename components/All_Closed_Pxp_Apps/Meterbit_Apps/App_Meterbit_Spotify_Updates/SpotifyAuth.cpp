// ===== File: SpotifyAuth.cpp =====
#include <HTTPClient.h>
#include "SpotifyAuth.h"
#include "mtbSpotifyInfo.h"
#include "mtb_nvs.h"

static const char TAG[] = "SPOTIFY_AUTH";

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
    ESP_LOGI(TAG, "HTTP POST failed: %d\n", httpCode);
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    ESP_LOGI(TAG, "Failed to parse JSON: %s\n", err.c_str());
    return false;
  }

  if (doc.containsKey("access_token")) {
    access_token = doc["access_token"].as<String>();

    if (doc.containsKey("refresh_token")) {
      String newRefreshToken = doc["refresh_token"].as<String>();
      strcpy(userSpotify.refreshToken, newRefreshToken.c_str());
      mtb_Write_Nvs_Struct("spotifyData", &userSpotify, sizeof(Spotify_Data_t));
      ESP_LOGI(TAG, "New refresh token saved.\n");
    }

    ESP_LOGI(TAG, "Access token obtained.\n");
    return true;
  }

  ESP_LOGI(TAG, "Failed to obtain token.\nBody: %s\n", body.c_str());
  return false;
}

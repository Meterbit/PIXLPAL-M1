#include <HTTPClient.h>
#include <ArduinoJson.h>

static const char TAG[] = "OUTLOOK_AUTH";

String getAccessToken(const String& clientId, const String& clientSecret, const String& refreshToken) {
  HTTPClient http;

  const char* tokenUrl = "https://oauth2.googleapis.com/token";
  String postData =
    "client_id=" + clientId +
    "&client_secret=" + clientSecret +
    "&refresh_token=" + refreshToken +
    "&grant_type=refresh_token";

  http.begin(tokenUrl);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int httpCode = http.POST(postData);
  String payload = http.getString();

  if (httpCode != 200) {
    ESP_LOGI(TAG, "HTTP Error: %d\n", httpCode);
    ESP_LOGI(TAG, "Response: %s\n", payload.c_str());
    http.end();
    return "";
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    ESP_LOGI(TAG, "JSON Parse Error while decoding token response\n");
    http.end();
    return "";
  }

  const char* token = doc["access_token"];
  if (token) {
    ESP_LOGI(TAG, "Access Token: %s\n", token);
    ESP_LOGI(TAG, "Token Expires In: %ld seconds\n", doc["expires_in"].as<long>());
    http.end();
    return String(token);
  } else {
    ESP_LOGI(TAG, "Access token missing in response\n");
    http.end();
    return "";
  }
}
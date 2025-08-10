#include <HTTPClient.h>
#include <ArduinoJson.h>

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
    printf("HTTP Error: %d\n", httpCode);
    printf("Response: %s\n", payload.c_str());
    http.end();
    return "";
  }

  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    printf("JSON Parse Error while decoding token response\n");
    http.end();
    return "";
  }

  const char* token = doc["access_token"];
  if (token) {
    printf("Access Token: %s\n", token);
    printf("Token Expires In: %ld seconds\n", doc["expires_in"].as<long>());
    http.end();
    return String(token);
  } else {
    printf("Access token missing in response\n");
    http.end();
    return "";
  }
}
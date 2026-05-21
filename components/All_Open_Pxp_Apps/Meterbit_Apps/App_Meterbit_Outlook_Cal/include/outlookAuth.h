#ifndef OUTLOOK_AUTH_H
#define OUTLOOK_AUTH_H

#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Arduino.h>

extern const char* token_host;
extern String access_token;
//bool getOutlookCalAccessToken(const char* client_id, const char* refresh_token);
extern char* googleCal_root_ca;


extern String getOutlookCalAccessToken(const String& clientId, const String& clientSecret, const String& refreshToken);

#endif // GOOGLE_AUTH_H

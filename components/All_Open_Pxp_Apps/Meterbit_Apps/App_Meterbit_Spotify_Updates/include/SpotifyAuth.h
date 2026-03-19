#ifndef SPOTIFY_AUTH_H
#define SPOTIFY_AUTH_H

#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

struct Spotify_Data_t {
char refreshToken[250] = {0};
};

extern Spotify_Data_t userSpotify;

extern const char* token_host;
extern String access_token;
bool getAccessToken(const char* client_id, const char* refresh_token);
extern char* spotify_root_ca;

#endif // SPOTIFY_AUTH_H

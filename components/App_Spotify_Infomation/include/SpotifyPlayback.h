#ifndef SPOTIFY_PLAYBACK_H
#define SPOTIFY_PLAYBACK_H

#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

void getNowPlaying();
void sendPlaybackCommand(const String& command, const String& method);

#endif // SPOTIFY_PLAYBACK_H

#ifndef SPOTIFY_APP
#define SPOTIFY_APP

// #include <stdio.h>
// #include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char spotifyAppRoute[] = "9/3";

struct Spotify_Data_t {
char refreshToken[250] = {0};
};

extern Spotify_Data_t userSpotify;
extern TaskHandle_t spotify_Task_H;
extern void spotify_App_Task(void *arguments);

#endif
#ifndef GIF_LIB
#define GIF_LIB

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern uint8_t set_Pixel_Change_Duration;
extern uint8_t drawGIF(const char *dImagePath, uint16_t row, uint16_t column, uint32_t loopCounter);
#endif

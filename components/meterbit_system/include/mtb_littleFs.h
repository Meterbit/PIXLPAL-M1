#ifndef LITLFS_H
#define LITLFS_H

#include <stdio.h>
#include "esp_littlefs.h"
#include <FS.h>
#include <LittleFS.h>

extern esp_vfs_littlefs_conf_t myLittleFS;

extern void mtb_LittleFS_Init(void);
extern void mtb_LittleFS_DeInit(void);

extern int fsize(FILE *fp);

#endif
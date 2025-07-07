#ifndef LITLFS_H
#define LITLFS_H

#include <stdio.h>
#include "esp_littlefs.h"
#include <FS.h>
#include <LittleFS.h>

extern esp_vfs_littlefs_conf_t myLittleFS;

extern void init_LittleFS_Mem(void);
extern void de_init_LittleFS_Mem(void);

extern int fsize(FILE *fp);

#endif
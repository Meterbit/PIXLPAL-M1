/**
 * @file mtb_littleFs.h
 * @brief LittleFS filesystem mount/unmount API and file-size helper.
 *
 * Wraps `esp_littlefs` and the Arduino-ESP32 `LittleFS` object to mount the
 * on-chip flash partition labelled "spiffs" (used here as a LittleFS volume).
 * The configuration is exposed as `myLittleFS` for direct esp-idf VFS access
 * if needed. After `mtb_LittleFS_Init()` succeeds, POSIX file I/O on paths
 * beginning with `/littlefs/` works as expected.
 */
#ifndef LITLFS_H
#define LITLFS_H

#include <stdio.h>
#include "esp_littlefs.h"
#include <FS.h>
#include <LittleFS.h>

extern esp_vfs_littlefs_conf_t myLittleFS; /**< VFS configuration struct used to register the LittleFS mount point. */

/**
 * @brief Mount the LittleFS partition and register it with the ESP-IDF VFS.
 * @note  Sets the global `litFS_Ready` flag on success.
 */
extern void mtb_LittleFS_Init(void);

/**
 * @brief Unmount the LittleFS partition and unregister it from the VFS.
 */
extern void mtb_LittleFS_DeInit(void);

/**
 * @brief Return the byte size of an already-open file.
 * @param fp  Open file pointer (must be seekable).
 * @return Size in bytes, or -1 on error.
 */
extern int fsize(FILE *fp);

#endif

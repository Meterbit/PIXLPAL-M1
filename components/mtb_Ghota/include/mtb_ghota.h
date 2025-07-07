
#ifndef MTB_GHOTA_H
#define MTB_GHOTA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_ghota.h>
#include "scrollMsgs.h"

/* initialize our ghota config */
static const char github_Token[] = "aaa";
extern ghota_config_t ghconfig;
extern TaskHandle_t ota_Updating;
extern SemaphoreHandle_t ota_Update_Sem;
extern void ota_Update_Task(void *);

#ifdef __cplusplus
}
#endif

#endif
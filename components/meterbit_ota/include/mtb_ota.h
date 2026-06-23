/**
 * @file mtb_ota.h
 * @brief Over-the-air (OTA) firmware update API using the esp_ghota GitHub-hosted OTA library.
 *
 * Exposes two FreeRTOS task entry points for checking and performing OTA updates
 * from a GitHub release. ota_Update_Sem is given to trigger an update check; the
 * tasks handle downloading and flashing the firmware autonomously.
 */

#ifndef meterbit_ota_H
#define meterbit_ota_H

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_ghota.h>
#include "mtb_text_scroll.h"

/* initialize our ghota config */
//static const char github_Token[] = "insert your github PAT token here";

extern SemaphoreHandle_t ota_Update_Sem; /**< Binary semaphore; give to trigger an OTA update check. */

/**
 * @brief FreeRTOS task that polls GitHub releases and reports whether a newer firmware is available.
 * @param param  Unused task parameter.
 * @note  Give ota_Update_Sem to unblock this task and start a check.
 */
extern void ghota_Check_Update_Task(void *param);

/**
 * @brief FreeRTOS task that downloads and flashes the latest firmware release from GitHub.
 * @param param  Unused task parameter.
 * @note  Typically started after ghota_Check_Update_Task confirms an update is available.
 *        The device reboots automatically on successful flash.
 */
extern void ghota_Perform_Update_Task(void *param);

#ifdef __cplusplus
}
#endif

#endif

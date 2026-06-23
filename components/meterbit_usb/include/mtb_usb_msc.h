/**
 * @file mtb_usb_msc.h
 * @brief USB Mass Storage Class (MSC) host driver task handle.
 *
 * Exposes the task handle for the USB MSC host task and the `USB_MOUNTED_BIT`
 * event group flag that signals a USB drive has been mounted and is ready for
 * file operations.
 *
 * The USB MSC host task is responsible for:
 *   - Registering the ESP-IDF USB MSC host driver.
 *   - Waiting for a USB drive connection event.
 *   - Mounting the FAT filesystem exposed by the drive.
 *   - Setting `USB_MOUNTED_BIT` in `usb_event_group` to notify other tasks.
 */

#ifndef USB_MSC_H
#define USB_MSC_H

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USB_MOUNTED_BIT BIT0 /**< EventGroup bit set when a USB MSC drive is successfully mounted. */

extern TaskHandle_t usb_Mass_Storage_H; /**< Task handle for the USB MSC host driver task. */

#ifdef __cplusplus
}
#endif

#endif

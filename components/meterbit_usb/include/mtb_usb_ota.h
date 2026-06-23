/**
 * @file mtb_usb_ota.h
 * @brief USB Mass Storage OTA and SPIFFS update API.
 *
 * Provides firmware and filesystem update flows triggered by a USB MSC drive
 * (thumb drive) inserted into the device. The drive must contain a file named
 * `firmware.bin` for OTA or a `spiffs.bin` for the SPIFFS/LittleFS partition.
 *
 * Typical usage:
 *   1. The USB MSC host task detects the drive and sets `USB_MOUNTED_BIT`.
 *   2. Call `attemptUSB_FirmwareUpdate()` or `attemptUSB_SPIFFSUpdate()` to
 *      check for a matching image file and perform the update.
 *   3. On success the device restarts automatically.
 */
#ifndef USB_OTA_H
#define USB_OTA_H

#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_msc_host.h"
#include "esp_msc_ota.h"
#include "usb/usb_host.h"

extern TaskHandle_t usbFirmwareUpdate_H; /**< Task handle for the USB firmware update monitor task. */

/**
 * @brief FreeRTOS task that watches for a USB drive and triggers OTA.
 *
 * Waits for `USB_MOUNTED_BIT` in the USB event group, then calls
 * `attemptUSB_FirmwareUpdate()` and `attemptUSB_SPIFFSUpdate()` in sequence.
 * Self-suspends between drive events.
 *
 * @param arguments  Unused task parameter.
 */
extern void usbFirmwareUpdateTask(void *arguments);

/**
 * @brief Check for `firmware.bin` on the USB drive and flash it via esp_msc_ota.
 *
 * @return 1 if a new firmware image was successfully flashed, 0 otherwise.
 */
extern uint8_t attemptUSB_FirmwareUpdate(void);

/**
 * @brief Check for `spiffs.bin` on the USB drive and flash it to the SPIFFS partition.
 *
 * @return 1 if the filesystem image was successfully flashed, 0 otherwise.
 */
extern uint8_t attemptUSB_SPIFFSUpdate(void);

/**
 * @brief Write a binary image to the named flash partition from a memory buffer.
 *
 * Used internally by `attemptUSB_SPIFFSUpdate()` after loading `spiffs.bin` into RAM.
 *
 * @param data             Pointer to the binary image data.
 * @param data_size        Size of the image in bytes.
 * @param partition_label  Name of the target flash partition (e.g. "spiffs").
 * @return ESP_OK on success, or an esp_err_t error code on failure.
 */
extern esp_err_t flash_bin_to_spiffs_partition(const void *data, size_t data_size, const char *partition_label);

#endif

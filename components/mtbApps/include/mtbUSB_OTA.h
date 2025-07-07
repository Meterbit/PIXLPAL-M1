#ifndef USB_OTA_H
#define USB_OTA_H

#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_msc_host.h"
#include "esp_msc_ota.h"
#include "usb/usb_host.h"
//#include "esp_log.h"

extern uint8_t attemptUSB_OTA_Update(void);
extern esp_err_t flash_bin_to_spiffs_partition(const void *data, size_t data_size, const char *partition_label);

#endif
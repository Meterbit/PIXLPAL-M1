#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "usb/usb_host.h"

#if __has_include("usb/usb_host_msc.h")
  #include "usb/usb_host_msc.h"
  #define USBMSCFS_HAVE_USB_MSC 1
#elif __has_include("usb/msc_host.h")
  #include "usb/msc_host.h"
  #define USBMSCFS_HAVE_MSC_HOST 1
#else
  #error "No USB MSC host API header found. Install espressif/usb_host_msc component or ensure ESP-IDF provides usb/msc_host.h"
#endif

namespace usbmscfs {

esp_err_t begin(const char* mount_point = "/usb", int max_files = 4, size_t allocation_unit = 16 * 1024);
void end();
bool is_mounted();

FILE* open(const char* rel_path, const char* mode);
bool exists(const char* rel_path);
bool remove_file(const char* rel_path);
bool make_dir(const char* rel_path);
bool remove_dir(const char* rel_path);
esp_err_t get_capacity(uint64_t* bytes_out);

} // namespace usbmscfs

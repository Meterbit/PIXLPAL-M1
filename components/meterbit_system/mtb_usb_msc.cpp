/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <dirent.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/msc_host.h"
#include "usb/msc_host_vfs.h"
#include "ffconf.h"
#include "errno.h"
#include "driver/gpio.h"
#include "mtb_usb_msc.h"
#include "Arduino.h"
#include "mtb_engine.h"
#include "mtb_usb_fs.h"

#include "freertos/event_groups.h"

EventGroupHandle_t usb_event_group;

/**
 * @brief Application Queue and its messages ID
 */
static QueueHandle_t app_queue;
typedef struct {
    enum MYVALUES{
        APP_QUIT,                // Signals request to exit the application
        APP_DEVICE_CONNECTED,    // USB device connect event
        APP_DEVICE_DISCONNECTED, // USB device disconnect event
    } id;
    union {
        uint8_t new_dev_address; // Address of new USB device for APP_DEVICE_CONNECTED event if
    } data;
} app_message_t;

static const char *TAG = "USB FLSH DRV";
#define MNT_PATH         "/usb"     // Path in the Virtual File System, where the USB flash drive is going to be mounted
#define BUFFER_SIZE      4096       // The read/write performance can be improved with larger buffer for the cost of RAM, 4kB is enough for most usecases

EXT_RAM_BSS_ATTR TaskHandle_t usb_Mass_Storage_H = NULL;
msc_host_device_handle_t msc_device = NULL;
msc_host_vfs_handle_t vfs_handle = NULL;
static void msc_event_cb(const msc_host_event_t *event, void *arg);
void usb_Mass_Strg_Task(void *params);
//static void print_device_info(msc_host_device_info_t *info);

EXT_RAM_BSS_ATTR Mtb_Services *usb_Mass_Storage_Sv = new Mtb_Services(usb_Mass_Strg_Task, &usb_Mass_Storage_H, "USB Mass Strg", 4096, 2, pdFALSE, 1);

void usb_Mass_Strg_Task(void* d_Service){
    Mtb_Services *thisServ = (Mtb_Services *)d_Service;
    // Create FreeRTOS primitives
    app_queue = xQueueCreate(5, sizeof(app_message_t));
    assert(app_queue);

    // Create the event group if not already created
    if (!usb_event_group) {
        usb_event_group = xEventGroupCreate();
    }

    const usb_host_config_t host_config = { .intr_flags = ESP_INTR_FLAG_LEVEL1 };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    const msc_host_driver_config_t msc_config = {
        .create_backround_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .callback = msc_event_cb,
    };
    ESP_ERROR_CHECK(msc_host_install(&msc_config));

    ESP_LOGI(TAG, "\nWaiting for USB flash drive to be connected");

//************************************************************************************************************ */
    bool has_clients = true;

    while (MTB_SERV_IS_ACTIVE == pdTRUE) {
        uint32_t event_flags;
        usb_host_lib_handle_events(pdMS_TO_TICKS(200), &event_flags);

        // Release devices once all clients has deregistered
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            has_clients = false;
            if (usb_host_device_free_all() == ESP_OK) {
                break;
            };
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE && !has_clients) {
            break;
        }

        app_message_t msg;
        BaseType_t success = xQueueReceive(app_queue, &msg, pdMS_TO_TICKS(200));

            if(msg.id == (app_message_t::MYVALUES) 1 && success == pdTRUE){
            ESP_LOGI(TAG, "Preparing USB flash for mounting at: %s\n", MNT_PATH);

            // 1. MSC flash drive connected. Open it and map it to Virtual File System
            ESP_ERROR_CHECK(msc_host_install_device(msg.data.new_dev_address, &msc_device));
            const esp_vfs_fat_mount_config_t mount_config = {
                .format_if_mount_failed = false,
                .max_files = 3,
                .allocation_unit_size = 8192,
            };
            ESP_ERROR_CHECK(msc_host_vfs_register(msc_device, MNT_PATH, &mount_config, &vfs_handle));
            // Signal to others that USB is mounted
            xEventGroupSetBits(usb_event_group, USB_MOUNTED_BIT);
            ESP_LOGI(TAG, "USB flash drive mounted at %s\n", MNT_PATH);
            Mtb_Applications::usbPenDriveMounted = true;
            USBFS.begin(MNT_PATH);
            }
            else if(msg.id == (app_message_t::MYVALUES)2 && success == pdTRUE){
            ESP_LOGI(TAG, "USB flash drive disconnected\n");
            }
    }

        if(vfs_handle != NULL) msc_host_vfs_unregister(vfs_handle);
        if(msc_device != NULL) msc_host_uninstall_device(msc_device);
        USBFS.end();
        Mtb_Applications::usbPenDriveMounted = false;
    // Wait for all clients to deregister

        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

        // Release devices once all clients has deregistered
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            has_clients = false;
            if (usb_host_device_free_all() == ESP_OK) {
                //break;
            };
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE && !has_clients) {
            //break;
        }

    delay(10); // Give clients some time to uninstall
    ESP_LOGI(TAG, "Deinitializing USB");
    ESP_ERROR_CHECK(usb_host_uninstall());
    vQueueDelete(app_queue);

    mtb_End_This_Service(thisServ);
}


/**
 * @brief MSC driver callback
 *
 * Signal device connection/disconnection to the main task
 *
 * @param[in] event MSC event
 * @param[in] arg   MSC event data
 */
static void msc_event_cb(const msc_host_event_t *event, void *arg)
{
    if (event->event == 0) {
        ESP_LOGI(TAG, "MSC device connected");
        app_message_t msg = {
            (app_message_t::MYVALUES) 1,
            1
        };
        Mtb_Applications::usbPenDriveConnectStatus = true;
        xQueueSend(app_queue, &msg, portMAX_DELAY);
    } else if (event->event == 1) {
        ESP_LOGI(TAG, "MSC device disconnected");
        app_message_t message = {
            .id = (app_message_t::MYVALUES) 2,
        };
        Mtb_Applications::usbPenDriveConnectStatus = false;
        xQueueSend(app_queue, &message, portMAX_DELAY);
    }
}

// static void print_device_info(msc_host_device_info_t *info)
// {
//     const size_t megabyte = 1024 * 1024;
//     uint64_t capacity = ((uint64_t)info->sector_size * info->sector_count) / megabyte;

//     ESP_LOGI(TAG, "Device info:\n");
//     ESP_LOGI(TAG, "\t Capacity: %llu MB\n", capacity);
//     ESP_LOGI(TAG, "\t Sector size: %"PRIu32"\n", info->sector_size);
//     ESP_LOGI(TAG, "\t Sector count: %"PRIu32"\n", info->sector_count);
//     ESP_LOGI(TAG, "\t PID: 0x%04X \n", info->idProduct);
//     ESP_LOGI(TAG, "\t VID: 0x%04X \n", info->idVendor);
// #ifndef CONFIG_NEWLIB_NANO_FORMAT
//     wprintf(L"\t iProduct: %S \n", info->iProduct);
//     wprintf(L"\t iManufacturer: %S \n", info->iManufacturer);
//     wprintf(L"\t iSerialNumber: %S \n", info->iSerialNumber);
// #endif
// }

// void file_operations(void){
//     const char *directory = "/usb/esp";
//     const char *file_path = "/usb/esp/test.txt";

//     app_message_t msg;
//     xQueueReceive(app_queue, &msg, portMAX_DELAY);

//         // 1. MSC flash drive connected. Open it and map it to Virtual File System
//         ESP_ERROR_CHECK(msc_host_install_device(msg.data.new_dev_address, &msc_device));
//         const esp_vfs_fat_mount_config_t mount_config = {
//             .format_if_mount_failed = false,
//             .max_files = 3,
//             .allocation_unit_size = 8192,
//         };
//         ESP_ERROR_CHECK(msc_host_vfs_register(msc_device, MNT_PATH, &mount_config, &vfs_handle));

//         // 2. Print information about the connected disk
//         msc_host_device_info_t info;
//         ESP_ERROR_CHECK(msc_host_get_device_info(msc_device, &info));
//         msc_host_print_descriptors(msc_device);
//         print_device_info(&info);

//     // Create /usb/esp directory
//     struct stat s = {0};
//     bool directory_exists = stat(directory, &s) == 0;
//     if (!directory_exists) {
//         if (mkdir(directory, 0775) != 0) {
//             ESP_LOGE(TAG, "mkdir failed with errno: %s", strerror(errno));
//         }
//     }

//     // Create /usb/esp/test.txt file, if it doesn't exist
//     if (stat(file_path, &s) != 0) {
//         ESP_LOGI(TAG, "Creating file");
//         FILE *f = fopen(file_path, "w");
//         if (f == NULL) {
//             ESP_LOGE(TAG, "Failed to open file for writing");
//             return;
//         }
//         fprintf(f, "Hello World!\n");
//         fclose(f);
//     }

//     // Read back the file
//     FILE *f;
//     ESP_LOGI(TAG, "Reading file");
//     f = fopen(file_path, "r");
//     if (f == NULL) {
//         ESP_LOGE(TAG, "Failed to open file for reading");
//         return;
//     }
//     char line[64];
//     fgets(line, sizeof(line), f);
//     fclose(f);
//     // strip newline
//     char *pos = strchr(line, '\n');
//     if (pos) {
//         *pos = '\0';
//     }
//     ESP_LOGI(TAG, "Read from file '%s': '%s'", file_path, line);
// }

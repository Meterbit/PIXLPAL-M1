#include "USBMSCFS.h"
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "msc_host_vfs.h"

static const char* TAG = "USBMSCFS";

namespace usbmscfs {

static char s_mount_point[16] = "/usb";
static bool s_initialized = false;
static bool s_mounted = false;
static TaskHandle_t s_usb_host_task = nullptr;

#if defined(USBMSCFS_HAVE_USB_MSC)
static usb_msc_device_handle_t s_dev = nullptr;
static usb_msc_vfs_handle_t s_vfs = nullptr;
#elif defined(USBMSCFS_HAVE_MSC_HOST)
static msc_host_device_handle_t s_dev = nullptr;
static msc_host_vfs_handle_t s_vfs = nullptr;
#endif

static void join_path(char* out, size_t out_len, const char* rel) {
    if (!rel || !*rel) { strlcpy(out, s_mount_point, out_len); return; }
    if (rel[0] == '/') rel++;
    snprintf(out, out_len, "%s/%s", s_mount_point, rel);
}

static void usb_host_events_task(void* arg) {
    uint32_t flags;
    while (true) {
        esp_err_t err = usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (err != ESP_OK) {
            // Keep running; errors can be transient
        }
    }
}

#if defined(USBMSCFS_HAVE_USB_MSC)
// New API uses callback + usb_msc_handle_events()
static volatile bool s_connected = false;
static void msc_conn_cb(usb_msc_event_t event, void* arg) {
    if (event == MSC_DEVICE_CONNECTED) s_connected = true;
}

static esp_err_t do_mount(int max_files, size_t allocation_unit) {
    usb_msc_driver_config_t cfg = {
        .conn_callback = msc_conn_cb,
        .conn_callback_arg = nullptr,
        .create_background_task = false,
        .task_priority = 5,
        .task_stack_size = 4096,
        .task_core_id = tskNO_AFFINITY,
    };
    esp_err_t err = usb_msc_install(&cfg);
    if (err != ESP_OK) return err;

    // Wait for a device to connect
    while (!s_connected) {
        usb_msc_handle_events(1000);
    }

    err = usb_msc_install_device(&s_dev);
    if (err != ESP_OK) return err;

    esp_vfs_fat_mount_config_t mnt = {
        .format_if_mount_failed = false,
        .max_files = max_files,
        .allocation_unit_size = allocation_unit,
    };
    err = usb_msc_vfs_register(s_dev, s_mount_point, &mnt, &s_vfs);
    if (err != ESP_OK) return err;

    s_mounted = true;
    return ESP_OK;
}
#elif defined(USBMSCFS_HAVE_MSC_HOST)
// Legacy API: Poll for a device address using USB Host core API
static esp_err_t do_mount(int max_files, size_t allocation_unit) {
    msc_host_driver_config_t cfg = {}; // zero-init default
    esp_err_t err = msc_host_install(&cfg);
    if (err != ESP_OK) return err;

    // Poll device list until at least one address appears
    ESP_LOGI(TAG, "Waiting for USB MSC device...");
    uint8_t addr_list[4] = {0};
    int num = 0;
    while (true) {
        err = usb_host_device_addr_list_fill(4, addr_list, &num);
        if (err == ESP_OK && num > 0) break;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    uint8_t dev_addr = addr_list[0];

    // Install MSC device by address
    err = msc_host_install_device(dev_addr, &s_dev);
    if (err != ESP_OK) return err;

    esp_vfs_fat_mount_config_t mnt = {
        .format_if_mount_failed = false,
        .max_files = max_files,
        .allocation_unit_size = allocation_unit,
    };
    err = msc_host_vfs_register(s_dev, s_mount_point, &mnt, &s_vfs);
    if (err != ESP_OK) return err;

    s_mounted = true;
    return ESP_OK;
}
#endif

esp_err_t begin(const char* mount_point, int max_files, size_t allocation_unit) {
    if (s_initialized) return ESP_OK;
    if (mount_point && *mount_point) {
        size_t n = strnlen(mount_point, sizeof(s_mount_point)-1);
        memcpy(s_mount_point, mount_point, n);
        s_mount_point[n] = '\0';
    }

    usb_host_config_t host_cfg = { .skip_phy_setup = false, .intr_flags = ESP_INTR_FLAG_LEVEL1 };
    esp_err_t err = usb_host_install(&host_cfg);
    if (err != ESP_OK) return err;

    if (xTaskCreatePinnedToCore(usb_host_events_task, "usb_host_events", 4096, nullptr, 5, &s_usb_host_task, tskNO_AFFINITY) != pdPASS) {
        usb_host_uninstall();
        return ESP_ERR_NO_MEM;
    }

    err = do_mount(max_files, allocation_unit);
    if (err != ESP_OK) {
        if (s_usb_host_task) vTaskDelete(s_usb_host_task);
        s_usb_host_task = nullptr;
        usb_host_uninstall();
        return err;
    }

    s_initialized = true;
    return ESP_OK;
}

void end() {
    if (!s_initialized) return;

    if (s_mounted) {
#if defined(USBMSCFS_HAVE_USB_MSC)
        if (s_vfs) { usb_msc_vfs_unregister(s_vfs); s_vfs = nullptr; }
        if (s_dev) { usb_msc_uninstall_device(s_dev); s_dev = nullptr; }
        usb_msc_uninstall();
#elif defined(USBMSCFS_HAVE_MSC_HOST)
        if (s_vfs) { msc_host_vfs_unregister(s_vfs); s_vfs = nullptr; }
        if (s_dev) { msc_host_uninstall_device(s_dev); s_dev = nullptr; }
        msc_host_uninstall();
#endif
        s_mounted = false;
    }

    if (s_usb_host_task) { vTaskDelete(s_usb_host_task); s_usb_host_task = nullptr; }
    usb_host_uninstall();
    s_initialized = false;
}

bool is_mounted() { return s_mounted; }

FILE* open(const char* rel_path, const char* mode) {
    if (!s_mounted) return nullptr;
    char full[256]; join_path(full, sizeof(full), rel_path);
    return ::fopen(full, mode);
}

bool exists(const char* rel_path) {
    if (!s_mounted) return false;
    char full[256]; join_path(full, sizeof(full), rel_path);
    struct stat st; return ::stat(full, &st) == 0;
}

bool remove_file(const char* rel_path) {
    if (!s_mounted) return false;
    char full[256]; join_path(full, sizeof(full), rel_path);
    return ::unlink(full) == 0;
}

bool make_dir(const char* rel_path) {
    if (!s_mounted) return false;
    char full[256]; join_path(full, sizeof(full), rel_path);
    return ::mkdir(full, 0777) == 0;
}

bool remove_dir(const char* rel_path) {
    if (!s_mounted) return false;
    char full[256]; join_path(full, sizeof(full), rel_path);
    return ::rmdir(full) == 0;
}

esp_err_t get_capacity(uint64_t* bytes_out) {
    if (!s_mounted || !bytes_out) return ESP_ERR_INVALID_STATE;
#if defined(USBMSCFS_HAVE_USB_MSC)
    usb_msc_device_info_t info = {};
    esp_err_t err = usb_msc_get_device_info(s_dev, &info);
#elif defined(USBMSCFS_HAVE_MSC_HOST)
    msc_host_device_info_t info = {};
    esp_err_t err = msc_host_get_device_info(s_dev, &info);
#endif
    if (err != ESP_OK) return err;
    *bytes_out = (uint64_t)info.sector_count * (uint64_t)info.sector_size;
    return ESP_OK;
}

} // namespace usbmscfs

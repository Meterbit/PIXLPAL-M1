#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gifdec.h"
#include "nvsMem.h"
#include "littleFSMgr.h"

static const char *TAG = "demo_esp_littlefs";

esp_vfs_littlefs_conf_t myLittleFS = {
            .base_path = "/littlefs",
            .partition_label = "spiffs",
            .format_if_mount_failed = pdFALSE,
            .dont_mount = false,
            .grow_on_mount = true,
        };


void init_LittleFS_Mem(void){
    // Use settings defined above to initialize and mount LittleFS filesystem.
    // Note: esp_vfs_littlefs_register is an all-in-one convenience function.
    // esp_err_t ret = esp_vfs_littlefs_register(&myLittleFS);

    if (!LittleFS.begin(true)) {
        printf("An error has occurred while mounting LittleFS..");
        return;
    }

//     if (ret != ESP_OK){
//             if (ret == ESP_FAIL)
//             {
//                     ESP_LOGE(TAG, "Failed to mount or format filesystem");
//             }
//             else if (ret == ESP_ERR_NOT_FOUND)
//             {
//                     ESP_LOGE(TAG, "Failed to find LittleFS partition");
//             }
//             else
//             {
//                     ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
//             }
//             return;
//     } //else ESP_LOGI(TAG, "MOUNTING OF LittleFS partition WAS SUCCESSFUL..");

    size_t total = 0, used = 0;
    esp_err_t ret = esp_littlefs_info(myLittleFS.partition_label, &total, &used);
    if (ret != ESP_OK){
            ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    }
    else{
            ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    if(ret == ESP_OK){
        if(esp_littlefs_mounted("spiffs")) ESP_LOGI("mtbLittleFS","LITTLEFS PARTITION SUCCESSFULLY MOUNTED....");
        else ESP_LOGI("mtbLittleFS","LITTLEFS PARTITION MOUNTING FAILED....");
    }else{
        litFS_Ready = pdFALSE;
        write_struct_to_nvs("litFS_Ready", &litFS_Ready, sizeof(uint8_t));

        esp_err_t ret1 = esp_littlefs_format("spiffs");
        myLittleFS.format_if_mount_failed = pdTRUE;
        // esp_err_t ret1 = esp_vfs_spiffs_register(&mySpiff);
        // esp_err_t ret2 = esp_spiffs_mounted("spiffs_1");
        esp_vfs_littlefs_register(&myLittleFS);
        if(esp_littlefs_mounted("spiffs")) ESP_LOGI("mtbLittleFS","LITTLEFS PARTITION SUCCESSFULLY MOUNTED....");
        else ESP_LOGI("mtbLittleFS","LITTLEFS PARTITION MOUNTING FAILED....");
    }
}

void de_init_LittleFS_Mem(void){
    esp_vfs_littlefs_unregister("spiffs");
}

int fsize(FILE *fp){
    int prev=ftell(fp);
    fseek(fp, 0L, SEEK_END);
    size_t sz=ftell(fp);
    fseek(fp,prev,SEEK_SET); //go back to where we were
    return sz;
}

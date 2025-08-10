#include <stdio.h>
#include <stdlib.h>
#include <time.h>
//#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_spi_flash.h"
#include "Arduino.h"
#include "mtb_engine.h"
#include "commonApps.h"
#include "mtb_text_scroll.h"
#include "mtb_ota.h"
#include "mtb_usb_ota.h"
#include "mtb_wifi.h"
#include "mtb_gif_parser.h"
#include "mtb_usb_fs.h"
#include "mtb_buzzer.h"

static const char *TAG = "usb_msc_ota";
#define OTA_FILE_NAME "/usb/PIXLPAL-M1.bin"

EXT_RAM_BSS_ATTR TaskHandle_t otaFailed_MQTT_Parser_Task_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t firmwareUpdate_H = NULL;

EXT_RAM_BSS_ATTR Applications_FullScreen *firmwareUpdate_App = new Applications_FullScreen(firmwareUpdateTask, &firmwareUpdate_H, "OTA FW UPDATE", 6144, pdFALSE, 1);

static void msc_ota_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
uint8_t attemptUSB_FirmwareUpdate(void);
uint8_t attemptUSB_SPIFFSUpdate(void);
void startButton_USB_OTA_UPDATE(button_event_t);
void startEncoder_USB_SPIFFS_UPDATE(rotary_encoder_rotation_t);

//***************************************************************************************************
void  firmwareUpdateTask(void* dApplication){
    Applications *thisApp = (Applications *) dApplication;
    thisApp->app_ButtonFn_ptr = startButton_USB_OTA_UPDATE;
    thisApp->app_EncoderFn_ptr = startEncoder_USB_SPIFFS_UPDATE;
    appsInitialization(thisApp);
    
//****************************************************************************************************************************
    FixedText_t otaTextTop(16, 18, Terminal8x12);
    FixedText_t otaTextBotm(16, 31, Terminal8x12);
//****************************************************************************************************************************
    
    if(litFS_Ready){
        uint8_t countdown = panelBrightness, countup = 0;
        // drawGIF("/mtblg/mtbStart.gif", 0, 0, 1);
        // delay(1000);
        // while (countdown-- > 0){
        //     dma_display->setBrightness(countdown);
        //     delay(10);
        // }      
        drawLocalPNG({"/mtblg/pixlpal.png", 0, 0});
        while(countup++ < panelBrightness){
            dma_display->setBrightness(countup);
            delay(20);
        }
    }
    set_Status_RGB_LED(MAGENTA);
do{
    if(Applications::firmwareOTA_Status > 6){
        printf("Code entered USB Firmware Update\n");
        dma_display->clearScreen();
        uint8_t attemptResult = attemptUSB_FirmwareUpdate();
        if(attemptResult == pdPASS) attemptUSB_SPIFFSUpdate();
        else break;
        Applications::firmwareOTA_Status = pdTRUE;
    } else if (Applications::spiffsOTA_Status > 6){
        printf("Code entered USB Firmware Update\n");
        dma_display->clearScreen();
        start_This_Service(usb_Mass_Storage_Sv);
        uint8_t attemptResult = attemptUSB_SPIFFSUpdate();
        if(attemptResult != pdPASS) break;
        Applications::spiffsOTA_Status = pdTRUE;
    } else delay(1000);
} while (Applications::firmwareOTA_Status--> pdTRUE && Applications::spiffsOTA_Status--> pdTRUE);

    set_Status_RGB_LED(YELLOW);

//************************************************************************************************************************************
    Applications::firmwareOTA_Status = pdFALSE;
    Applications::spiffsOTA_Status = pdFALSE;

    while (THIS_APP_IS_ACTIVE == pdTRUE) delay(2000);
    kill_This_App(thisApp);
}

static void msc_ota_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
//***************************************************************************************************
    static FixedText_t usbOTA_Text(8, 39, Terminal8x12, LEMON_YELLOW);
    static FixedText_t otaProgressText(8, 52, Terminal8x12, GREEN);
    static FixedText_t otaProgressPercentage(95, 52, Terminal8x12, WHITE);

    static char softProgress[12] = {0};

    switch (event_id) {
    case ESP_MSC_OTA_START:
        ESP_LOGI(TAG, "ESP_MSC_OTA_START");
        break;
    case ESP_MSC_OTA_READY_UPDATE:
        ESP_LOGI(TAG, "ESP_MSC_OTA_READY_UPDATE");
            if(litFS_Ready){
            PNG_LocalImage_t otaUSB_Image {"/batIcons/otaUSB.png", 0, 0};
            drawLocalPNG(otaUSB_Image);
            }
            usbOTA_Text.writeString("SOFTWARE UPDATE");
            otaProgressText.writeString("PROGRESS: ");
        break;
    case ESP_MSC_OTA_WRITE_FLASH:{
        float progress = *(float *)event_data;
        int completed = progress * 100;
        sprintf(softProgress, "%d", completed);
        strcat(softProgress, "%");
        otaProgressPercentage.writeString(softProgress);
        break;}
    case ESP_MSC_OTA_FAILED:
        ESP_LOGI(TAG, "ESP_MSC_OTA_FAILED");
        otaProgressText.writeColoredString("FAILED... ", RED);
        break;
    case ESP_MSC_OTA_GET_IMG_DESC:
        ESP_LOGI(TAG, "ESP_MSC_OTA_GET_IMG_DESC");
        break;
    case ESP_MSC_OTA_VERIFY_CHIP_ID:{
        esp_chip_id_t chip_id = *(esp_chip_id_t *)event_data;
        ESP_LOGI(TAG, "ESP_MSC_OTA_VERIFY_CHIP_ID, chip_id: %08x", chip_id);
        break;}
    case ESP_MSC_OTA_UPDATE_BOOT_PARTITION:{
        esp_partition_subtype_t subtype = *(esp_partition_subtype_t *)event_data;
        ESP_LOGI(TAG, "ESP_MSC_OTA_UPDATE_BOOT_PARTITION, subtype: %d", subtype);
        break;}
    case ESP_MSC_OTA_FINISH:
        ESP_LOGI(TAG, "ESP_MSC_OTA_FINISH");
        otaProgressText.writeColoredString("SUCCESSFUL", CYAN);
        break;
    case ESP_MSC_OTA_ABORT:
        ESP_LOGI(TAG, "ESP_MSC_OTA_ABORT");
        otaProgressText.writeColoredString("ABORTED... ", ORANGE);
        break;
    }
}

uint8_t attemptUSB_FirmwareUpdate(void){
    //****************************************************************************************************************************
    esp_event_loop_create_default();
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_MSC_OTA_EVENT, ESP_EVENT_ANY_ID, &msc_ota_event_handler, NULL));
    esp_msc_host_config_t msc_host_config = {
        .base_path = "/usb",
        .host_config = DEFAULT_USB_HOST_CONFIG(),
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
    };
    esp_msc_host_handle_t host_handle = NULL;
    esp_msc_host_install(&msc_host_config, &host_handle);

    esp_msc_ota_config_t config = {
        .ota_bin_path = OTA_FILE_NAME,
        .wait_msc_connect = pdMS_TO_TICKS(1000),
    };
#if CONFIG_SIMPLE_MSC_OTA
    esp_err_t ret = esp_msc_ota(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_msc_ota failed, ret: %d", ret);
    }
#else
    esp_msc_ota_handle_t msc_ota_handle = NULL;

    esp_err_t ret = esp_msc_ota_begin(&config, &msc_ota_handle);
    ESP_GOTO_ON_ERROR(ret, fail, TAG, "esp_msc_ota_begin failed, err: %d", ret);

    do {
        ret = esp_msc_ota_perform(msc_ota_handle);
        if (ret != ESP_OK) {
            break;
            ESP_LOGE(TAG, "esp_msc_ota_perform: (%s)\n", esp_err_to_name(ret));
        }
    } while (!esp_msc_ota_is_complete_data_received(msc_ota_handle));

    if (esp_msc_ota_is_complete_data_received(msc_ota_handle)) {
        esp_msc_ota_end(msc_ota_handle);
        ESP_LOGI(TAG, "esp msc ota complete");
        delay(3000);
        esp_restart();
        return pdPASS; 
    } else {
        esp_msc_ota_abort(msc_ota_handle);
        ESP_LOGE(TAG, "esp msc ota failed");
    }
fail:
#endif
    esp_msc_host_uninstall(host_handle);
    return pdFAIL;   
//***************************************************************************************************************************
}

uint8_t attemptUSB_SPIFFSUpdate(void){
uint8_t checkMountAttempts = 5;

while (checkMountAttempts-- > 0) {
    if (Applications::usbPenDriveConnectStatus == true && Applications::usbPenDriveMounted == true) {  
        ESP_LOGI(TAG, "USB is mounted, proceeding with SPIFFS update");
        
        File file = USBFS.open("/spiffs.bin", "r");
        if (!file) {
        ESP_LOGI(TAG, "Open for read failed");
        return pdFAIL;
        }

        // 3. Determine size and allocate PSRAM buffer
        size_t fileSize = file.size();
        ESP_LOGI(TAG, "File size: %u bytes\n", (unsigned)fileSize);

        // Allocate an extra byte if you want to NUL-terminate text
        uint8_t* psramBuf = (uint8_t*) heap_caps_malloc(fileSize, 
                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (!psramBuf) {
        ESP_LOGI(TAG, "PSRAM allocation failed");
            file.close();
            return pdFAIL;
        }

        // 4. Read the file in one shot (or loop if you want chunked reads)
        size_t readBytes = file.read(psramBuf, fileSize);
        file.close();

        if (readBytes != fileSize) {
        ESP_LOGI(TAG, "Only read %u / %u bytes\n",
                        (unsigned)readBytes, (unsigned)fileSize);
            heap_caps_free(psramBuf);
            return pdFAIL;
        } ESP_LOGI(TAG, "Read complete bytes\n");

        ESP_LOGI(TAG, "File loaded into PSRAM!");

        // Function to flash a .BIN file from RAM to a specified SPIFFS partition
        esp_err_t result = flash_bin_to_spiffs_partition(psramBuf, readBytes, "spiffs");
        if (result == ESP_OK){
           ESP_LOGI(TAG, "SPIFFS partition updated successfully");
           do_beep(BEEP_1);
        } 
        else ESP_LOGE(TAG, "Failed to update SPIFFS partition: %s", esp_err_to_name(result));

        heap_caps_free(psramBuf);    // free when done

        break;
    }
    delay(1000);
}


return pdPASS;
}

// Function to flash a .BIN file from RAM to a specified SPIFFS partition
esp_err_t flash_bin_to_spiffs_partition(const void *data, size_t data_size, const char *partition_label) {
    // Find the partition
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, partition_label);
    if (!partition) {
        ESP_LOGE(TAG, "Partition not found");
        return ESP_FAIL;
    }

    // Erase the partition
    esp_err_t err = esp_partition_erase_range(partition, 0, (data_size + SPI_FLASH_SEC_SIZE - 1) & ~(SPI_FLASH_SEC_SIZE - 1));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase partition: %s", esp_err_to_name(err));
        return err;
    }

    // Write the data to the partition
    err = esp_partition_write(partition, 0, data, data_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write to partition: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Data written to SPIFFS partition successfully");
    return ESP_OK;
}

void startButton_USB_OTA_UPDATE (button_event_t button_Data){
            switch (button_Data.type){
            case BUTTON_RELEASED:
            break;

            case BUTTON_PRESSED:
            Applications::firmwareOTA_Status = 10;
            do_beep(CLICK_BEEP);
            break;

            case BUTTON_PRESSED_LONG:
            break;

            case BUTTON_CLICKED:
            //printf("Button Clicked: %d Times\n",button_Data.count);
            switch (button_Data.count){
            case 1:
                break;
            case 2:
                break;
            case 3:
                break;
            default:
                break;
            }
                break;
            default:
            break;
			}
}

void startEncoder_USB_SPIFFS_UPDATE(rotary_encoder_rotation_t direction){
if (direction == ROT_CLOCKWISE){
    Applications::spiffsOTA_Status = 10;
    do_beep(CLICK_BEEP);
} else if(direction == ROT_COUNTERCLOCKWISE){
    Applications::spiffsOTA_Status = 10;
    do_beep(CLICK_BEEP);
}
}

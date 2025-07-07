idf_component_register(SRCS "pixelAnimClock.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 mtbCore Wifi_Man ESP32-HUB75-MatrixPanel-DMA "json" "lwip" "mbedtls" "nvs_flash" mtbApps ESP32-audioI2S mtbAudio
                    )

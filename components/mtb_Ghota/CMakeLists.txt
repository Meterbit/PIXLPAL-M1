idf_component_register(SRCS "mtb_ghota.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 mtbCore Wifi_Man ESP32-HUB75-MatrixPanel-DMA "json" "lwip" "mbedtls" "nvs_flash" mtbApps esp_ghota
                    )

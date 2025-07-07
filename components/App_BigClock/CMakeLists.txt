idf_component_register(SRCS "bigClockCalendar.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 mtbCore ESP32-HUB75-MatrixPanel-DMA "json" "lwip" "mbedtls" "nvs_flash" mtbApps
                    )

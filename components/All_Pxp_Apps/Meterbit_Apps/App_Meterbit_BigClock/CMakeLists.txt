idf_component_register(SRCS "mtb_bigClock.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 meterbit_system ESP32-HUB75-MatrixPanel-DMA "json" "lwip" "mbedtls" "nvs_flash" meterbit_engine
                    )

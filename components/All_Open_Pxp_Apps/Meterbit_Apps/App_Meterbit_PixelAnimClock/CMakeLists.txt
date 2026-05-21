idf_component_register(SRCS "pixelAnimClock.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 meterbit_system meterbit_wifi "json" "lwip" "mbedtls" "nvs_flash" meterbit_engine ESP32-audioI2S meterbit_audio
                    WHOLE_ARCHIVE)

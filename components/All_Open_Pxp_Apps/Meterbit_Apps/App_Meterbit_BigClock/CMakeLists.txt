idf_component_register(SRCS "mtb_bigClock.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 meterbit_system "json" "lwip" "mbedtls" "nvs_flash" meterbit_engine
                    WHOLE_ARCHIVE)

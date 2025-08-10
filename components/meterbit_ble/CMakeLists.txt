idf_component_register(SRCS "mtb_ble_software_update.cpp" "mtb_ble_settings.cpp" "mtb_ble_wifi.cpp" "mtb_ble_system.cpp" "mtb_ble.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 meterbit_system meterbit_engine esp_ghota meterbit_ota ArduinoJson-7xx esp-nimble-cpp meterbit_ble meterbit_wifi "json" "lwip" "mbedtls" "nvs_flash" "esp_app_format"
                    )

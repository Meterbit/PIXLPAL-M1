idf_component_register(SRCS "mtb_buzzer.cpp" "mtb_littleFs.cpp" "mtb_system.cpp" "mtb_nvs.cpp" "mtb_ntp.cpp" "mtb_usb_fs.cpp" "mtb_usb_msc.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES "nvs_flash" "wpa_supplicant" "lwip" "app_update" "esp_http_server" "esp_http_client" "esp_https_ota" arduino-esp32 espressif__usb_host_msc ebtn meterbit_wifi fishwaldo__esp_ghota meterbit_ota meterbit_engine joltwallet__littlefs meterbit_ble meterbit_github ArduinoJson-7xx
                    )

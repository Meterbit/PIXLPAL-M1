idf_component_register(SRCS "softwareUpdate.cpp" "bleSettings.cpp" "wifiSettings.cpp" "systemSettings.cpp" "mtbBLE_Control.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 mtbCore mtbApps esp_ghota mtb_Ghota ArduinoJson-7xx esp-nimble-cpp mtbBLE_Control Wifi_Man "json" "lwip" "mbedtls" "nvs_flash" "esp_app_format"
                    )

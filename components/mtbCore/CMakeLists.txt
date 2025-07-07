idf_component_register(SRCS "littleFSMgr.cpp" "systemm.cpp" "ledPanel.cpp" "beep.cpp"  "gif_Lib.cpp" "ntpTime.cpp" "nvsMem.cpp" "gifdec.c" "scrollMsgs.cpp" "lodepng.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES "nvs_flash" "wpa_supplicant" "lwip" "app_update" "esp_http_server" "esp_http_client" "esp_https_ota"   ESP32-HUB75-MatrixPanel-DMA arduino-esp32 espressif__usb_host_msc ebtn Wifi_Man fishwaldo__esp_ghota mtb_Ghota mtbApps joltwallet__littlefs mtbBLE_Control mtbGithubStorage nanosvg
                    )

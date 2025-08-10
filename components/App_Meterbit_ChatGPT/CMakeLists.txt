idf_component_register(SRCS "mtb_chatGPT.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 meterbit_system meterbit_wifi ESP32-HUB75-MatrixPanel-DMA "json" "lwip" "mbedtls" "nvs_flash" meterbit_engine ESP32-audioI2S meterbit_audio OpenAI-ESP32 meterbit_ble
                    )
            

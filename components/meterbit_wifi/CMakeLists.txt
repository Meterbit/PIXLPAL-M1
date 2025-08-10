idf_component_register(SRCS "mtb_wifi.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES meterbit_ota meterbit_mqtt meterbit_ble arduino-esp32 meterbit_engine "json"
                    )

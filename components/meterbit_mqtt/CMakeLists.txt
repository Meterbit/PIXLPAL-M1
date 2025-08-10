idf_component_register(SRCS "mtb_mqtt.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 PicoMQTT meterbit_system meterbit_engine "lwip" "mbedtls" ArduinoJson-7xx meterbit_github
                    )

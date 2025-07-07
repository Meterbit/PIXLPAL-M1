idf_component_register(SRCS "mtb_MQTT.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 PicoMQTT mtbCore mtbApps "lwip" "mbedtls" bblanchon__arduinojson mtbGithubStorage
                    )

idf_component_register(SRCS "Wifi_Man.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES mtbCore mtbMQTT mtbBLE_Control arduino-esp32 mtbApps "json"
                    )

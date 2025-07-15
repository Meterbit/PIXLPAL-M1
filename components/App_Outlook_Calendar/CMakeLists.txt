idf_component_register(SRCS "outlookAuth.cpp" "outlookCalendar.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 mtbCore ESP32-HUB75-MatrixPanel-DMA "lwip" mtbApps ArduinoJson-7xx
                    )
            

idf_component_register(SRCS "googleCalendar.cpp" "GoogleAuth.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 meterbit_system ESP32-HUB75-MatrixPanel-DMA "lwip" meterbit_engine ArduinoJson-7xx
                    )
            

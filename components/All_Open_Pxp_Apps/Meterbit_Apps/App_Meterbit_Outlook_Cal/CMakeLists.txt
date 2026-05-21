idf_component_register(SRCS "outlookAuth.cpp" "outlookCalendar.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 meterbit_system "lwip" meterbit_engine ArduinoJson-7xx
                    WHOLE_ARCHIVE)
            

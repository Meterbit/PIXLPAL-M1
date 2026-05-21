idf_component_register(SRCS "exampleDrawImageApp.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 meterbit_graphics meterbit_engine meterbit_system ArduinoJson-7xx
                    WHOLE_ARCHIVE)

idf_component_register(SRCS "exampleUseEncoderBeepApp.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 meterbit_engine meterbit_system meterbit_graphics ArduinoJson-7xx
                    WHOLE_ARCHIVE)

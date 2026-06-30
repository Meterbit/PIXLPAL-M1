idf_component_register(SRCS "aiPixelArt.cpp"
                    REQUIRES arduino-esp32 meterbit_engine meterbit_system meterbit_graphics ArduinoJson-7xx mbedtls
                    WHOLE_ARCHIVE)

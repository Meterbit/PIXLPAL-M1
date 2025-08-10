idf_component_register(SRCS "openMeteo.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 meterbit_engine meterbit_system ArduinoJson-7xx
                    )

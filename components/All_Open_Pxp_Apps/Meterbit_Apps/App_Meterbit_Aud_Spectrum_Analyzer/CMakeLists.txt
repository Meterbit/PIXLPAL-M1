idf_component_register(SRCS "audSpectrumAnalyzer.cpp"
                    INCLUDE_DIRS "include"                
                    REQUIRES meterbit_system ArduinoJson-7xx arduino-esp32 ebtn ESP32-audioI2S meterbit_audio meterbit_engine ESP32-HUB75-MatrixPanel-DMA
                    )

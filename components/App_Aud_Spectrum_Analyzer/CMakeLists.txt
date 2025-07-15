idf_component_register(SRCS "audSpectrumAnalyzer.cpp"
                    INCLUDE_DIRS "include"                
                    REQUIRES mtbCore ArduinoJson-7xx arduino-esp32 ebtn ESP32-audioI2S mtbAudio mtbApps ESP32-HUB75-MatrixPanel-DMA
                    )

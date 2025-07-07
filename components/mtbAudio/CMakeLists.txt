idf_component_register(SRCS "mtb_Mic.cpp" "mtbI2SAudio.cpp" 
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES usb 
                    REQUIRES arduino-esp32 ESP32-HUB75-MatrixPanel-DMA arduinoFFT mtbCore ESP32-audioI2S mtbApps FastLED
                    )

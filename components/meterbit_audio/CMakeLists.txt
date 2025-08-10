idf_component_register(SRCS "mtb_Mic.cpp" "mtb_audio.cpp" 
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES usb 
                    REQUIRES arduino-esp32 ESP32-HUB75-MatrixPanel-DMA arduinoFFT meterbit_system meterbit_graphics ESP32-audioI2S meterbit_engine FastLED
                    )

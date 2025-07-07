idf_component_register(SRCS "SpotifyPlayback.cpp" "SpotifyAuth.cpp" "mtbSpotifyInfo.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 mtbCore ESP32-HUB75-MatrixPanel-DMA "lwip" mtbApps spotify-api bblanchon__arduinojson
                    )
            

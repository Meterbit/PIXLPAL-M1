idf_component_register(SRCS "SpotifyPlayback.cpp" "SpotifyAuth.cpp" "mtbSpotifyInfo.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES arduino-esp32 meterbit_system "lwip" meterbit_engine spotify-api ArduinoJson-7xx
                    WHOLE_ARCHIVE)
            

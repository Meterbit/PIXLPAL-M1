

idf_component_register(
                    SRCS "mtb_status_bar_calendar.cpp" "workerWorldFlagsFns.cpp" "mtb_status_bar_clock.cpp" "mtb_engine.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_timer meterbit_usb meterbit_system meterbit_graphics meterbit_mqtt ArduinoJson-7xx meterbit_github PicoMQTT arduino-esp32 ebtn meterbit_wifi meterbit_ota meterbit_ble
                    
################    Mtb_Applications START HERE
                    App_Meterbit_Apple_Notifications
                    App_Meterbit_Aud_Spectrum_Analyzer
                    App_Meterbit_BigClock
                    App_Meterbit_Calendar_Clock
                    App_Meterbit_ChatGPT
                    App_Meterbit_Crypto_Stats
                    App_Meterbit_Currency_Exchange
                    App_Meterbit_Finnhub
                    App_Meterbit_Google_Calendar
                    App_Meterbit_Google_News
                    App_Meterbit_Google_Weather
                    App_Meterbit_Internet_Radio
                    App_Meterbit_Live_Football
                    App_Meterbit_Music_Player
                    App_Meterbit_News_API
                    App_Meterbit_OpenMeteo
                    App_Meterbit_OpenWeather
                    App_Meterbit_Outlook_Cal
                    App_Meterbit_PixelAnimClock
                    App_Meterbit_PolygonFX
                    App_Meterbit_Rss_News_Reader
                    App_Meterbit_Spotify_Updates
                    App_Meterbit_Stopwatch
                    App_Meterbit_StudioLight
                    App_Meterbit_World_Clock   
                    App_Meterbit_World_Flags

################    Example App Placed Here for UI Testing and Development
                    App_Example_Design_Mobile_UI  
                    )


# Add the static library file to the component
#target_link_libraries(${COMPONENT_LIB} "${CMAKE_CURRENT_SOURCE_DIR}/libmeterbit_engine.a")
idf_component_register(SRCS "usb_msc.cpp" "firmwareOTA_Update.cpp" "classClk.cpp" "mtbApps.cpp" "statusBarClock.cpp"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES usb fatfs driver esp_timer
                    REQUIRES mtbCore mtbMQTT ArduinoJson-7xx mtbGithubStorage PicoMQTT arduino-esp32 ebtn Wifi_Man mtb_Ghota ESP32-audioI2S mtbAudio espressif__esp_msc_ota mtbBLE_Control ESP-Mail-Client
                    
################    APPLICATIONS START HERE
                    App_Apple_Notifications
                    App_Aud_Spectrum_Analyzer
                    App_BigClock
                    App_ChatGPT_ChatCompletions
                    App_Crypto_Stats
                    App_CurrencyExchange
                    App_Finnhub
                    App_Google_Calendar
                    App_Google_News
                    App_Internet_Radio
                    App_LiveFootball
                    App_Outlook_Calendar
                    App_PixelAnimClock
                    App_Rss_News_Reader
                    App_Spotify_Infomation
                    App_Stopwatch
                    App_StudioLight
                    App_OpenWeather
                    App_GoogleWeather
                    App_OpenMeteo
                    App_World_Clock
                    App_PolygonFX
                    App_WorldFlags
                    )

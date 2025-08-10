idf_component_register(SRCS "mtb_graphics.cpp" "mtb_text_scroll.cpp" "mtb_gif_parser.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES  ESP32-HUB75-MatrixPanel-DMA arduino-esp32 nanosvg gif_decoder lodepng meterbit_system
                    )

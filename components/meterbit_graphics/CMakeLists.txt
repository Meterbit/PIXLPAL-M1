idf_component_register(
                    SRCS "lodepng.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES  ESP32-HUB75-MatrixPanel-DMA arduino-esp32 nanosvg gif_decoder meterbit_system
                    )

                    # Add the static library file to the component
target_link_libraries(${COMPONENT_LIB} "${CMAKE_CURRENT_SOURCE_DIR}/libmeterbit_graphics.a")
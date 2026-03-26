idf_component_register(
                    SRCS "lodepng_psram.cpp" "lodepng.cpp" "mtb_text_scroll.cpp" "mtb_graphics.cpp" 
                    INCLUDE_DIRS "include"
                    REQUIRES  arduino-esp32 nanosvg gif_decoder meterbit_system esphome__esp-hub75 meterbit_engine
                    )

                    # Add the static library file to the component
#target_link_libraries(${COMPONENT_LIB} "${CMAKE_CURRENT_SOURCE_DIR}/libmeterbit_graphics.a")
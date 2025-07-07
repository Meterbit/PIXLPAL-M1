# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_compile_definitions(CONFIG_SPIRAM_SUPPORT=1)
add_compile_definitions(NANOSVG_IMPLEMENTATION=1)
add_compile_definitions(NANOSVGRAST_IMPLEMENTATION=1)
add_compile_options(-DDISABLE_SMTP=0)
# add_compile_definitions(CONFIG_BLUEDROID_ENABLED=1)
project(PIXLPAL-M1)

# GFX_Root
# MIT License

cmake_minimum_required(VERSION 3.5)

set(GFXRoot_SRCS
  src/GFX.cpp
  src/glcdfont.c
  )

idf_component_register(SRCS ${GFXRoot_SRCS}
                       INCLUDE_DIRS "src"
                       REQUIRES arduino-esp32 FastLED)

project(GFX_Root)

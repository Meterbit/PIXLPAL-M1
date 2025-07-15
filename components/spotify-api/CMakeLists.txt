

FILE(GLOB_RECURSE app_sources ${dir}/src/*.cpp)

idf_component_register(SRCS ${app_sources}      
                    INCLUDE_DIRS "src"
                    REQUIRES arduino-esp32 ArduinoJson-7xx
)


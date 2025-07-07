

FILE(GLOB_RECURSE cpp_sources ${dir}/src/*.cpp)
FILE(GLOB_RECURSE c_sources ${dir}/src/*.c)

# Combine both variables into a single app_sources
set(app_sources ${cpp_sources} ${c_sources})


idf_component_register(SRCS ${app_sources}      
                    INCLUDE_DIRS "src"
                    REQUIRES arduino-esp32 tcp_transport esp_http_client json
)


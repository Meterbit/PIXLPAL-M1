idf_component_register( SRCS "main.cpp" 
                        INCLUDE_DIRS "." 
                        REQUIRES meterbit_system ebtn meterbit_audio meterbit_wifi meterbit_graphics "nvs_flash" ESP-Mail-Client
                        
                        )

#                        littlefs_create_partition_image(spiffs sysLitFs FLASH_IN_PROJECT)
#                        littlefs_create_partition_image(userDat userLitFs FLASH_IN_PROJECT)

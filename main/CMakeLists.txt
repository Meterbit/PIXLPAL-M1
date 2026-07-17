idf_component_register( SRCS "main.cpp" 
                        INCLUDE_DIRS "." 
                        REQUIRES meterbit_system ebtn meterbit_audio meterbit_wifi meterbit_graphics "nvs_flash" esp-nimble-cpp

                        App_TemplateApp
                        App_Example_Write_Text
                        App_Example_Draw_Shapes
                        App_Example_Draw_Local_Image
                        App_Example_Encoder_N_Beep
                        App_Example_Draw_Online_Image
                        )

                         littlefs_create_partition_image(spiffs sysLitFs)
                         littlefs_create_partition_image(userDat userLitFs)

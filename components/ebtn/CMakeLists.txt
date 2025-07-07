idf_component_register(
    SRCS "encoder.c" "button.c"
    INCLUDE_DIRS "include"
    REQUIRES mtbCore freertos driver
    PRIV_REQUIRES log esp_timer 
)
idf_component_register(
    SRCS "config_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_common nvs_flash
    PRIV_REQUIRES beacon_animation
)

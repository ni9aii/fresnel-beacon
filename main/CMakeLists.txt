idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES led_driver beacon_animation ipc config_manager wifi_manager
)

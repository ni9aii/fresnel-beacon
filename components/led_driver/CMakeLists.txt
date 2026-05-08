idf_component_register(
    SRCS "led_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_rmt freertos
)

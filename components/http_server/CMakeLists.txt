idf_component_register(
    SRCS "http_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server config_manager ipc wifi_manager json ota_manager
)

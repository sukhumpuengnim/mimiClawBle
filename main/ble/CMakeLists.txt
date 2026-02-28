idf_component_register(
    SRCS "ble_cli.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash esp_timer
)

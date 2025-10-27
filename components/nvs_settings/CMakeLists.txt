idf_component_register(
    SRCS "nvs_settings.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi
)
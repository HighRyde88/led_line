idf_component_register(
    SRCS  "wifi.c" "dwnvs.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi esp_timer nvs_flash mdns_service esp_http_client
)
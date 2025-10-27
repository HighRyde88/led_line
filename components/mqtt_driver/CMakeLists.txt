idf_component_register(
    SRCS  "mqtt.c" 
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_settings mqtt_driver esp_netif esp_wifi json mqtt lwip esp_timer
)
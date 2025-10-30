idf_component_register(
    SRCS 
        "captive_portal.c"
        "server/dns.c"
        "server/http.c"
        "server/websocket.c"

        "server/modules/wifi.c"
        "server/modules/supportive.c"
        "server/modules/mqtt.c"
        "server/modules/update.c"
        "server/modules/device.c"
        "server/modules/network.c"
        "server/modules/apoint.c"
        "server/modules/control.c"
        "server/modules/ledstrip.c"            
        "server/modules/data_parser.c"

        
    INCLUDE_DIRS 
        "."

    EMBED_FILES 
        "assets/index.html"
        "assets/styles.css"
        "assets/loader.js"
        "assets/settings_core.js"
        "assets/base_module.js"
        "assets/wifi_module.js"
        "assets/control_module.js"
        "assets/update_module.js"
        "assets/device_module.js"
        "assets/modules_registry.js"
        "assets/network_module.js"
        "assets/apoint_module.js"
        "assets/mqtt_module.js"
        "assets/ledstrip_module.js"
        "assets/script.js"

    REQUIRES 
        mbedtls

    PRIV_REQUIRES 
        nvs_settings
        wifi_driver
        mqtt_driver
        esp_http_server
        esp_https_ota
        esp_wifi
        efuse
        esp_timer
        lwip
        esp_netif
        json
        mqtt
        driver
)
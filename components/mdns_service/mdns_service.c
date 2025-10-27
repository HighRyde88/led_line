#include "mdns.h"
#include "esp_log.h"
#include "string.h"

static const char *TAG = "mDNS";
static bool mdns_ready = false;

//=================================================================
esp_err_t init_mdns_service(const char *hostname, const char *instance)
{
    // Проверка аргументов
    if (hostname == NULL || instance == NULL) {
        ESP_LOGE(TAG, "Invalid arguments: hostname or instance is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (strlen(hostname) == 0 || strlen(instance) == 0) {
        ESP_LOGE(TAG, "Invalid arguments: hostname or instance is empty");
        return ESP_ERR_INVALID_ARG;
    }

    if (mdns_ready) {
        ESP_LOGW(TAG, "mDNS already initialized");
        return ESP_OK;
    }

    // Инициализация mDNS
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Установка hostname
    err = mdns_hostname_set(hostname);
    if (err) {
        ESP_LOGE(TAG, "mDNS hostname set failed: %s", esp_err_to_name(err));
        mdns_free();
        return err;
    }

    // Установка instance name
    err = mdns_instance_name_set(instance);
    if (err) {
        ESP_LOGE(TAG, "mDNS instance name set failed: %s", esp_err_to_name(err));
        mdns_free();
        return err;
    }

    mdns_ready = true;
    ESP_LOGI(TAG, "mDNS initialized with hostname: %s, instance: %s", hostname, instance);
    return ESP_OK;
}

//=================================================================
esp_err_t add_http_service(const char *service_name, uint16_t port)
{
    if (!mdns_ready) {
        ESP_LOGE(TAG, "mDNS not initialized. Call init_mdns_service() first");
        return ESP_ERR_INVALID_STATE;
    }

    if (port == 0) {
        ESP_LOGE(TAG, "Invalid port: %d", port);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = mdns_service_add(service_name, "_http", "_tcp", port, NULL, 0);
    if (err) {
        ESP_LOGE(TAG, "mDNS add HTTP service failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "mDNS HTTP service added: %s on port %d", service_name, port);
    return ESP_OK;
}

//=================================================================
esp_err_t add_custom_service(const char *service_name, const char *service_type, 
                            const char *proto, uint16_t port, 
                            mdns_txt_item_t *txt_data, size_t txt_count)
{
    if (!mdns_ready) {
        ESP_LOGE(TAG, "mDNS not initialized. Call init_mdns_service() first");
        return ESP_ERR_INVALID_STATE;
    }

    if (service_name == NULL || service_type == NULL || proto == NULL) {
        ESP_LOGE(TAG, "Invalid service parameters");
        return ESP_ERR_INVALID_ARG;
    }

    if (port == 0) {
        ESP_LOGE(TAG, "Invalid port: %d", port);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = mdns_service_add(service_name, service_type, proto, port, txt_data, txt_count);
    if (err) {
        ESP_LOGE(TAG, "mDNS add service failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "mDNS service added: %s.%s.%s on port %d", service_name, service_type, proto, port);
    return ESP_OK;
}

//=================================================================
esp_err_t remove_service(const char *service_type, const char *proto)
{
    if (!mdns_ready) {
        ESP_LOGE(TAG, "mDNS not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (service_type == NULL || proto == NULL) {
        ESP_LOGE(TAG, "Invalid service parameters");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = mdns_service_remove(service_type, proto);
    if (err) {
        ESP_LOGE(TAG, "mDNS remove service failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "mDNS service removed: %s.%s", service_type, proto);
    return ESP_OK;
}

//=================================================================
void deinit_mdns_service(void)
{
    if (mdns_ready) {
        mdns_free();
        mdns_ready = false;
        ESP_LOGI(TAG, "mDNS service deinitialized");
    }
}

//=================================================================
bool is_mdns_ready(void)
{
    return mdns_ready;
}
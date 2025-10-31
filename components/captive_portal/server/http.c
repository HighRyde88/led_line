#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"

static httpd_handle_t http_server = NULL;

// Основные файлы
extern const unsigned char index_html_start[] asm("_binary_index_html_start");
extern const unsigned char index_html_end[] asm("_binary_index_html_end");

extern const unsigned char styles_css_start[] asm("_binary_styles_css_start");
extern const unsigned char styles_css_end[] asm("_binary_styles_css_end");

extern const unsigned char script_js_start[] asm("_binary_script_js_start");
extern const unsigned char script_js_end[] asm("_binary_script_js_end");

// Модульные JS-файлы
extern const unsigned char settings_core_js_start[] asm("_binary_settings_core_js_start");
extern const unsigned char settings_core_js_end[] asm("_binary_settings_core_js_end");

extern const unsigned char base_module_js_start[] asm("_binary_base_module_js_start");
extern const unsigned char base_module_js_end[] asm("_binary_base_module_js_end");

extern const unsigned char wifi_module_js_start[] asm("_binary_wifi_module_js_start");
extern const unsigned char wifi_module_js_end[] asm("_binary_wifi_module_js_end");

extern const unsigned char control_module_js_start[] asm("_binary_control_module_js_start");
extern const unsigned char control_module_js_end[] asm("_binary_control_module_js_end");

extern const unsigned char update_module_js_start[] asm("_binary_update_module_js_start");
extern const unsigned char update_module_js_end[] asm("_binary_update_module_js_end");

extern const unsigned char device_module_js_start[] asm("_binary_device_module_js_start");
extern const unsigned char device_module_js_end[] asm("_binary_device_module_js_end");

extern const unsigned char network_module_js_start[] asm("_binary_network_module_js_start");
extern const unsigned char network_module_js_end[] asm("_binary_network_module_js_end");

extern const unsigned char apoint_module_js_start[] asm("_binary_apoint_module_js_start");
extern const unsigned char apoint_module_js_end[] asm("_binary_apoint_module_js_end");

extern const unsigned char mqtt_module_js_start[] asm("_binary_mqtt_module_js_start");
extern const unsigned char mqtt_module_js_end[] asm("_binary_mqtt_module_js_end");

extern const unsigned char modules_registry_js_start[] asm("_binary_modules_registry_js_start");
extern const unsigned char modules_registry_js_end[] asm("_binary_modules_registry_js_end");

extern const unsigned char ledstrip_module_js_start[] asm("_binary_ledstrip_module_js_start");
extern const unsigned char ledstrip_module_js_end[] asm("_binary_ledstrip_module_js_end");

extern const unsigned char loader_js_start[] asm("_binary_loader_js_start");
extern const unsigned char loader_js_end[] asm("_binary_loader_js_end");

//=================================================================
// Обработчики основных файлов
static esp_err_t captive_portal_handler(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
    return ESP_OK;
}

static esp_err_t style_css_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char *)styles_css_start, styles_css_end - styles_css_start);
    return ESP_OK;
}

static esp_err_t script_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)script_js_start, script_js_end - script_js_start);
    return ESP_OK;
}

// Обработчики модульных JS-файлов
static esp_err_t modules_registry_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)modules_registry_js_start, modules_registry_js_end - modules_registry_js_start);
    return ESP_OK;
}
static esp_err_t settings_core_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)settings_core_js_start, settings_core_js_end - settings_core_js_start);
    return ESP_OK;
}

static esp_err_t loader_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)loader_js_start, loader_js_end - loader_js_start);
    return ESP_OK;
}

static esp_err_t base_module_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)base_module_js_start, base_module_js_end - base_module_js_start);
    return ESP_OK;
}

static esp_err_t wifi_module_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)wifi_module_js_start, wifi_module_js_end - wifi_module_js_start);
    return ESP_OK;
}

static esp_err_t control_module_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)control_module_js_start, control_module_js_end - control_module_js_start);
    return ESP_OK;
}

static esp_err_t update_module_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)update_module_js_start, update_module_js_end - update_module_js_start);
    return ESP_OK;
}

static esp_err_t device_module_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)device_module_js_start, device_module_js_end - device_module_js_start);
    return ESP_OK;
}

static esp_err_t network_module_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)network_module_js_start, network_module_js_end - network_module_js_start);
    return ESP_OK;
}

static esp_err_t apoint_module_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)apoint_module_js_start, apoint_module_js_end - apoint_module_js_start);
    return ESP_OK;
}

static esp_err_t mqtt_module_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)mqtt_module_js_start, mqtt_module_js_end - mqtt_module_js_start);
    return ESP_OK;
}

static esp_err_t ledstrip_module_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)ledstrip_module_js_start, ledstrip_module_js_end - ledstrip_module_js_start);
    return ESP_OK;
}

//=================================================================
void captive_portal_http_server_start(esp_netif_t *netif)
{
    if (netif == NULL)
    {
        ESP_LOGE("HTTPD", "Invalid netif provided");
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 7;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 19;
    config.stack_size = 8192;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;

    if (http_server)
    {
        httpd_stop(http_server);
        http_server = NULL;
    }

    if (httpd_start(&http_server, &config) == ESP_OK)
    {
        ESP_LOGI("HTTPD", "HTTP server started successfully on port %d", config.server_port);

        httpd_uri_t uris[] = {

            {.uri = "/", .method = HTTP_GET, .handler = captive_portal_handler, .user_ctx = netif},
            {.uri = "/generate_204", .method = HTTP_GET, .handler = captive_portal_handler, .user_ctx = netif},
            {.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = captive_portal_handler, .user_ctx = netif},
            {.uri = "/connecttest.txt", .method = HTTP_GET, .handler = captive_portal_handler, .user_ctx = netif},
            {.uri = "/redirect", .method = HTTP_GET, .handler = captive_portal_handler, .user_ctx = netif},

            // Статические ресурсы
            {.uri = "/styles.css", .method = HTTP_GET, .handler = style_css_handler, .user_ctx = netif},
            {.uri = "/script.js", .method = HTTP_GET, .handler = script_js_handler, .user_ctx = netif},

            // Модульные JS-файлы
            {.uri = "/settings_core.js", .method = HTTP_GET, .handler = settings_core_js_handler, .user_ctx = netif},
            {.uri = "/base_module.js", .method = HTTP_GET, .handler = base_module_js_handler, .user_ctx = netif},
            {.uri = "/wifi_module.js", .method = HTTP_GET, .handler = wifi_module_js_handler, .user_ctx = netif},
            {.uri = "/control_module.js", .method = HTTP_GET, .handler = control_module_js_handler, .user_ctx = netif},
            {.uri = "/update_module.js", .method = HTTP_GET, .handler = update_module_js_handler, .user_ctx = netif},
            {.uri = "/device_module.js", .method = HTTP_GET, .handler = device_module_js_handler, .user_ctx = netif},
            {.uri = "/network_module.js", .method = HTTP_GET, .handler = network_module_js_handler, .user_ctx = netif},
            {.uri = "/apoint_module.js", .method = HTTP_GET, .handler = apoint_module_js_handler, .user_ctx = netif},
            {.uri = "/mqtt_module.js", .method = HTTP_GET, .handler = mqtt_module_js_handler, .user_ctx = netif},
            {.uri = "/ledstrip_module.js", .method = HTTP_GET, .handler = ledstrip_module_js_handler, .user_ctx = netif},
            {.uri = "/modules_registry.js", .method = HTTP_GET, .handler = modules_registry_js_handler, .user_ctx = netif},
            {.uri = "/loader.js", .method = HTTP_GET, .handler = loader_js_handler, .user_ctx = netif},
        };

        for (int i = 0; i < sizeof(uris) / sizeof(uris[0]); i++)
        {
            httpd_register_uri_handler(http_server, &uris[i]);
        }

        // Уменьшаем уровень логирования HTTPD
        esp_log_level_set("httpd", ESP_LOG_ERROR);
    }
    else
    {
        ESP_LOGE("HTTPD", "Failed to start HTTP server");
        http_server = NULL;
    }
}

//=================================================================
esp_err_t captive_portal_http_server_stop(void)
{
    if (http_server)
    {
        httpd_stop(http_server);
        http_server = NULL;
    }
    return ESP_OK;
}
//=================================================================
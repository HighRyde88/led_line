#include "nvs_settings.h"
#include "wifi.h"
#include "dwnvs.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "server/server.h"
#include "esp_err.h"

#define PORTAL_LOGOUT (BIT1)

static const char *TAG = "PORTAL";
static EventGroupHandle_t portal_status = NULL;

// Определяем тип колбэка
typedef esp_err_t (*portal_sta_connect_attempt_cb_t)(void);

//=================================================================
static esp_err_t access_point_start(const char *ssid, const char *password)
{
    if (ssid == NULL || strlen(ssid) == 0)
    {
        ESP_LOGE(TAG, "SSID не может быть пустым");
        return ESP_ERR_INVALID_ARG;
    }

    wifi_ap_config_t ap_config = {0};

    size_t ssid_len = strlen(ssid);
    size_t max_ssid_len = sizeof(ap_config.ssid) - 1;

    if (ssid_len > max_ssid_len)
    {
        ssid_len = max_ssid_len;
        ESP_LOGW(TAG, "SSID обрезан до %zu символов", max_ssid_len);
    }

    memcpy(ap_config.ssid, ssid, ssid_len);
    ap_config.ssid[ssid_len] = '\0';
    ap_config.ssid_len = ssid_len;

    bool has_valid_password = false;
    if (password != NULL && strlen(password) > 0)
    {
        size_t pwd_len = strlen(password);
        size_t max_pwd_len = sizeof(ap_config.password) - 1;

        if (pwd_len >= 8 && pwd_len <= 63)
        {
            if (pwd_len > max_pwd_len)
            {
                pwd_len = max_pwd_len;
                ESP_LOGW(TAG, "Пароль обрезан до %zu символов", max_pwd_len);
            }
            memcpy(ap_config.password, password, pwd_len);
            ap_config.password[pwd_len] = '\0';
            has_valid_password = true;
        }
        else
        {
            ESP_LOGW(TAG, "Недопустимая длина пароля: %zu (нужно 8–63)", pwd_len);
        }
    }

    ap_config.authmode = has_valid_password ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    ap_config.max_connection = 1;
    ap_config.channel = 11;
    ap_config.ssid_hidden = 0;

    esp_err_t err = dw_access_point_start(&ap_config);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "AP запущен: SSID='%s', auth=%s, ch=%d",
                 ap_config.ssid,
                 has_valid_password ? "WPA2" : "OPEN",
                 ap_config.channel);
    }
    else
    {
        ESP_LOGE(TAG, "Ошибка запуска AP: %s", esp_err_to_name(err));
    }

    return err;
}

//=================================================================
static void portal_server_start(void)
{
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    captive_portal_dns_server_start(ap_netif);
    captive_portal_http_server_start(ap_netif);
    captive_portal_ws_server_start(ap_netif);
}

//=================================================================
void portal_start_with_sta_attempt(const char *ssid, const char *password, bool start_ap, portal_sta_connect_attempt_cb_t try_sta_connect)
{
    char standalone[8] = {0};
    size_t str_size = sizeof(standalone);
    esp_err_t err = nvs_load_data("wifi", "standalone", standalone, &str_size, NVS_TYPE_STR);

    bool ap_mode_active = false;
    bool standalone_mode = (err == ESP_OK && strcmp(standalone, "true") == 0) ? true : false;

    if (!start_ap && standalone_mode)
        return;

    dw_resources_init();

    if (portal_status == NULL)
    {
        portal_status = xEventGroupCreate();
        if (portal_status == NULL)
        {
            ESP_LOGE(TAG, "Failed to create portal event status");
        }
    }

    // 1. Проверяем необходимость запуска AP
    bool ap_required = start_ap;

    if (nvs_is_flag_set("is_reboot"))
    {
        nvs_clear_flag("is_reboot");
        ap_required = true; // При перезагрузке всегда запускаем AP
    }

    // Если AP требуется, сразу запускаем его
    if (ap_required)
    {
        ESP_LOGI(TAG, "AP required. Starting AP...");
        if (ssid != NULL && strlen(ssid) > 0)
        {
            ESP_LOGI(TAG, "Starting AP with provided SSID: %s", ssid);
            if (access_point_start(ssid, password) == ESP_OK)
            {
                ESP_LOGI(TAG, "AP started successfully. Starting portal server.");
                portal_server_start();
                ap_mode_active = true;
            }
            else
            {
                ESP_LOGW(TAG, "Failed to start AP with provided SSID. Trying fallback AP.");
            }
        }

        // Попробовать загрузить AP-конфигурацию из NVS
        if (!ap_mode_active)
        {
            wifi_ap_config_t ap_config = {0};
            if (dwnvs_load_ap_config(&ap_config) == ESP_OK && ap_config.ssid[0] != '\0')
            {
                ESP_LOGI(TAG, "Starting AP with saved config: %s", ap_config.ssid);
                if (access_point_start((const char *)ap_config.ssid,
                                       (const char *)ap_config.password) == ESP_OK)
                {
                    ESP_LOGI(TAG, "AP started with saved config. Starting portal server.");
                    portal_server_start();
                    ap_mode_active = true;
                }
            }
        }

        // Fallback AP
        if (!ap_mode_active)
        {
            ESP_LOGI(TAG, "Starting fallback AP: PortalFallback");
            if (access_point_start("PortalFallback", NULL) == ESP_OK)
            {
                ESP_LOGI(TAG, "Fallback AP started. Starting portal server.");
                portal_server_start();
                ap_mode_active = true;
            }
            else
            {
                ESP_LOGE(TAG, "Failed to start fallback AP.");
            }
        }
    }

    // 2. Если AP не требуется и standalone_mode == true, подключение как STA не производится
    if (!ap_required && !standalone_mode)
    {
        if (try_sta_connect == NULL)
        {
            ESP_LOGI(TAG, "try_sta_connect is NULL. Skipping STA connection, starting AP as fallback.");
        }
        else
        {
            ESP_LOGI(TAG, "try_sta_connect is provided. Attempting to connect as STA...");

            esp_err_t sta_result = try_sta_connect();

            if (sta_result == ESP_OK)
            {
                ESP_LOGI(TAG, "STA connection successful via callback. Returning.");
                return; // Успешно подключились — выходим
            }
            else
            {
                ESP_LOGW(TAG, "STA connection failed via callback. Starting AP as fallback.");
            }
        }

        // 3. Если STA не удался или колбэк не был вызван, запускаем AP
        ESP_LOGI(TAG, "STA connection failed or not attempted. Starting AP as fallback.");
        if (ssid != NULL && strlen(ssid) > 0)
        {
            ESP_LOGI(TAG, "Starting AP with provided SSID: %s", ssid);
            if (access_point_start(ssid, password) == ESP_OK)
            {
                ESP_LOGI(TAG, "Fallback AP started. Starting portal server.");
                portal_server_start();
                ap_mode_active = true;
            }
        }

        // Загрузить и использовать AP-конфигурацию из NVS
        if (!ap_mode_active)
        {
            wifi_ap_config_t ap_config = {0};
            if (dwnvs_load_ap_config(&ap_config) == ESP_OK && ap_config.ssid[0] != '\0')
            {
                ESP_LOGI(TAG, "Starting AP with saved config: %s", ap_config.ssid);
                if (access_point_start((const char *)ap_config.ssid,
                                       (const char *)ap_config.password) == ESP_OK)
                {
                    ESP_LOGI(TAG, "AP started with saved config. Starting portal server.");
                    portal_server_start();
                    ap_mode_active = true;
                }
            }
        }

        // Fallback AP
        if (!ap_mode_active)
        {
            ESP_LOGI(TAG, "Starting fallback AP: PortalFallback");
            if (access_point_start("PortalFallback", NULL) == ESP_OK)
            {
                ESP_LOGI(TAG, "Fallback AP started. Starting portal server.");
                portal_server_start();
                ap_mode_active = true;
            }
            else
            {
                ESP_LOGE(TAG, "Failed to start fallback AP.");
            }
        }
    }

    // Если AP успешно запущен, входим в бесконечный цикл (если не standalone)
    if (ap_mode_active)
    {
        ESP_LOGI(TAG, "AP mode active - entering infinite loop...");
        while (1)
        {
            EventBits_t bits = xEventGroupWaitBits(portal_status, PORTAL_LOGOUT, true, pdTRUE, 0);
            if (bits & PORTAL_LOGOUT)
            {
                err = nvs_load_data("wifi", "standalone", standalone, &str_size, NVS_TYPE_STR);
                standalone_mode = (err == ESP_OK && strcmp(standalone, "true") == 0) ? true : false;
                
                if(!standalone_mode && try_sta_connect != NULL)
                {
                    try_sta_connect();
                }

                break;
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

//=================================================================
void portal_stop(bool stop_sta)
{
    ESP_LOGI(TAG, "Stopping DNS server...");
    captive_portal_dns_server_stop();
    ESP_LOGI(TAG, "DNS server stopped.");

    ESP_LOGI(TAG, "Stopping HTTP server...");
    captive_portal_http_server_stop();
    ESP_LOGI(TAG, "HTTP server stopped.");

    ESP_LOGI(TAG, "Stopping WebSocket server...");
    captive_portal_ws_server_stop();
    ESP_LOGI(TAG, "WebSocket server stopped.");

    if (stop_sta)
    {
        ESP_LOGI(TAG, "Stopping Wi-Fi Station mode...");
        dw_station_stop();
    }

    ESP_LOGI(TAG, "Stopping Access Point mode...");
    dw_access_point_stop();

    ESP_LOGI(TAG, "Cleanup Wi-Fi module...");
    dw_wifi_cleanup();

    ESP_LOGI(TAG, "Portal fully stopped.");

    xEventGroupSetBits(portal_status, PORTAL_LOGOUT);
}
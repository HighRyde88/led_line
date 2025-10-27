#include "nvs_settings.h"
#include "wifi.h"
#include "dwnvs.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "server/server.h"
#include "esp_err.h"

#define CAPTIVE_PORTAL_LOGOUT (BIT1)

static const char *TAG = "CAPP";
static EventGroupHandle_t captive_status = NULL;
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
    ap_config.channel = 0;
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
static void captive_portal_server_start(void)
{
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    captive_portal_dns_server_start(ap_netif);
    captive_portal_http_server_start(ap_netif);
    captive_portal_ws_server_start(ap_netif);
}

//=================================================================
static void dw_sta_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        char hostname[32] = {0};
        size_t hostname_len = sizeof(hostname);
        if (nvs_load_data("device", "hostname", hostname, &hostname_len, NVS_TYPE_STR) == ESP_OK)
        {
            dw_set_hostname_to_netif(WIFI_IF_STA, hostname);
        }
    }
}

//=================================================================
void captive_portal_start(const char *ssid, const char *password, bool start_ap)
{
    dw_resources_init();
    bool ap_mode_active = false; // Флаг для отслеживания режима AP

    if (captive_status == NULL)
    {
        captive_status = xEventGroupCreate();
        if (captive_status == NULL)
        {
            ESP_LOGE(TAG, "Failed to create captive event status");
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
                ESP_LOGI(TAG, "AP started successfully. Starting captive portal server.");
                captive_portal_server_start();
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
                    ESP_LOGI(TAG, "AP started with saved config. Starting captive portal server.");
                    captive_portal_server_start();
                    ap_mode_active = true;
                }
            }
        }

        // Fallback AP
        if (!ap_mode_active)
        {
            ESP_LOGI(TAG, "Starting fallback AP: CaptivePortal");
            if (access_point_start("CaptivePortal", NULL) == ESP_OK)
            {
                ESP_LOGI(TAG, "Fallback AP started. Starting captive portal server.");
                captive_portal_server_start();
                ap_mode_active = true;
            }
            else
            {
                ESP_LOGE(TAG, "Failed to start fallback AP.");
            }
        }
    }

    // 2. Если AP не требуется, пробуем подключиться как STA
    if (!ap_required)
    {
        wifi_sta_config_t sta_config = {0};
        bool sta_config_loaded = (dwnvs_load_sta_config(&sta_config) == ESP_OK);

        if (sta_config_loaded)
        {
            ESP_LOGI(TAG, "STA configuration loaded. Attempting to connect to: %s", sta_config.ssid);
            if (dw_station_connect(&sta_config, dw_sta_event_handler, NULL) == ESP_OK)
            {
                ESP_LOGI(TAG, "Successfully started STA connection to: %s", sta_config.ssid);
                return; // Успешно подключились — выходим
            }
            else
            {
                ESP_LOGW(TAG, "Failed to start STA connection. Deleting config and starting AP.");
                dwnvs_delete_sta_config(); // Удаляем битую конфигурацию
            }
        }
        else
        {
            ESP_LOGI(TAG, "No STA configuration found. Starting AP.");
        }

        // 3. Если STA не удался, принудительно запускаем AP
        ESP_LOGI(TAG, "STA connection failed. Starting AP as fallback.");
        if (ssid != NULL && strlen(ssid) > 0)
        {
            ESP_LOGI(TAG, "Starting AP with provided SSID: %s", ssid);
            if (access_point_start(ssid, password) == ESP_OK)
            {
                ESP_LOGI(TAG, "Fallback AP started. Starting captive portal server.");
                captive_portal_server_start();
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
                    ESP_LOGI(TAG, "AP started with saved config. Starting captive portal server.");
                    captive_portal_server_start();
                    ap_mode_active = true;
                }
            }
        }

        // Fallback AP
        if (!ap_mode_active)
        {
            ESP_LOGI(TAG, "Starting fallback AP: CaptivePortal");
            if (access_point_start("CaptivePortal", NULL) == ESP_OK)
            {
                ESP_LOGI(TAG, "Fallback AP started. Starting captive portal server.");
                captive_portal_server_start();
                ap_mode_active = true;
            }
            else
            {
                ESP_LOGE(TAG, "Failed to start fallback AP.");
            }
        }
    }

    // Если AP успешно запущен, входим в бесконечный цикл
    if (ap_mode_active)
    {
        ESP_LOGI(TAG, "AP mode active - entering infinite loop...");
        while (1)
        {
            EventBits_t bits = xEventGroupWaitBits(captive_status, CAPTIVE_PORTAL_LOGOUT, true, pdTRUE, 0);
            if (bits & CAPTIVE_PORTAL_LOGOUT)
            {
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(1000)); // Делаем паузу 1 секунда, чтобы не нагружать CPU
        }
    }
}

//=================================================================
void captive_portal_stop(bool isStopSTA)
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

    if (isStopSTA)
    {
        ESP_LOGI(TAG, "Stopping Wi-Fi Station mode...");
        dw_station_stop();
    }

    ESP_LOGI(TAG, "Stopping Access Point mode...");
    dw_access_point_stop();

    ESP_LOGI(TAG, "Cleanup Wi-Fi module...");
    dw_wifi_cleanup();

    ESP_LOGI(TAG, "Captive portal fully stopped.");

    xEventGroupSetBits(captive_status, CAPTIVE_PORTAL_LOGOUT);
}
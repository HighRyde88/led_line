#include "esp_err.h"
#include "cJSON.h"
#include "nvs_settings.h"
#include "string.h"
#include "server/server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "captive_portal.h"
#include "esp_mac.h"
#include "esp_chip_info.h"
#include "esp_netif.h"
#include "dwnvs.h"
#include "modules.h"
#include "esp_app_desc.h"
#include "esp_bootloader_desc.h"
#include "data_parser.h"

#define DISCONNECT_BY_USER (BIT1)

static const char *TAG = "Wifi module";
static EventGroupHandle_t captive_wifi_group = NULL;

static const config_param_t wifi_params[] = {
    {"standalone", "standalone"},
};
//=================================================================
static void wifi_scan_done_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    uint16_t ap_count = 0;

    esp_err_t err = dw_station_scan_result(&ap_count, NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get scan result count: %s", esp_err_to_name(err));
        goto cleanup;
    }

    ESP_LOGI(TAG, "Scan completed: %u networks found", ap_count);

    cJSON *data_obj = cJSON_CreateObject();
    if (!data_obj)
    {
        ESP_LOGE(TAG, "Failed to create data object");
        goto cleanup;
    }

    cJSON_AddNumberToObject(data_obj, "count", ap_count);

    send_response_json("event", "wifi", "ap_scan_success", data_obj, true);

cleanup:
    return; // arg всегда NULL, освобождать нечего
}
//=================================================================
static void captive_sta_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    bool should_reconnect = false;
    bool *should_reconnect_ptr = (bool *)arg;
    if (should_reconnect_ptr != NULL)
    {
        should_reconnect = *should_reconnect_ptr;
    }
    static bool disconnect_notify = false;

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        char ssid_str[32] = {0};
        char ip_str[16] = {0};
        char gw_str[16] = {0};
        char nm_str[16] = {0};

        dw_station_set_auto_reconnect(true);

        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
        {
            strlcpy(ssid_str, (char *)ap_info.ssid, sizeof(ssid_str));
        }

        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
        {
            esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
            esp_ip4addr_ntoa(&ip_info.gw, gw_str, sizeof(gw_str));
            esp_ip4addr_ntoa(&ip_info.netmask, nm_str, sizeof(nm_str));
        }

        cJSON *data_obj = cJSON_CreateObject();
        if (data_obj)
        {
            cJSON_AddStringToObject(data_obj, "ssid", ssid_str);
            cJSON_AddStringToObject(data_obj, "ip", ip_str);
            cJSON_AddStringToObject(data_obj, "gateway", gw_str);
            cJSON_AddStringToObject(data_obj, "netmask", nm_str);

            send_response_json("event", "wifi", "ap_got_ip", data_obj, true);
    
        }
        else
        {
            ESP_LOGE(TAG, "Failed to create JSON response for got_ip event");
            return;
        }

        disconnect_notify = false;
        xEventGroupWaitBits(captive_wifi_group, DISCONNECT_BY_USER, true, pdTRUE, 0);

        char hostname[32] = {0};
        size_t hostname_len = sizeof(hostname);
        if (nvs_load_data("device", "hostname", hostname, &hostname_len, NVS_TYPE_STR) == ESP_OK)
        {
            dw_set_hostname_to_netif(WIFI_IF_STA, hostname);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        send_response_json("event", "wifi", "ap_wait_ip", NULL, false);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;

        EventBits_t bits = xEventGroupWaitBits(captive_wifi_group, DISCONNECT_BY_USER, false, pdTRUE, 0);
        if (bits & DISCONNECT_BY_USER)
        {
            disconn->reason = WIFI_REASON_UNSPECIFIED;
        }

        if (should_reconnect_ptr != NULL && should_reconnect == true)
        {
            if (disconnect_notify == false)
            {
                disconnect_notify = true;
                disconn->reason = WIFI_REASON_UNSPECIFIED;
            }
            else
            {
                return;
            }
        }

        cJSON *data_obj = cJSON_CreateObject();
        if (data_obj)
        {
            cJSON_AddNumberToObject(data_obj, "reason", disconn->reason);
            cJSON_AddStringToObject(data_obj, "reason_str", wifi_reason_to_string(disconn->reason));

            send_response_json("event", "wifi", "ap_disconnected_from_reason", data_obj, true);
    
        }
        else
        {
            ESP_LOGE(TAG, "Failed to serialize JSON for disconnected event");
        }
    }
}
//=================================================================
esp_err_t wifi_module_target(cJSON *json)
{
    cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
    if (!cJSON_IsString(action) || action->valuestring == NULL)
    {
        send_response_json("response", "wifi", "connect_ap_failed", "missing or invalid 'action'", false);
        return ESP_ERR_INVALID_ARG;
    }

    if (captive_wifi_group == NULL)
    {
        captive_wifi_group = xEventGroupCreate();
        if (captive_wifi_group == NULL)
        {
            ESP_LOGE(TAG, "Failed to create wifi event group");
        }
    }

    if (strcmp(action->valuestring, "ap_scan_start") == 0)
    {
        esp_err_t scan_err = dw_station_scan_start(wifi_scan_done_handler, NULL);

        if (scan_err == ESP_OK)
        {
            send_response_json("response", "wifi", "ap_scan_started", NULL, false);
        }
        else if (scan_err == ESP_ERR_INVALID_STATE)
        {
            send_response_json("response", "wifi", "ap_scan_already", NULL, false);
        }
        else
        {
            send_response_json("response", "wifi", "ap_scan_error", esp_err_to_name(scan_err), false);
        }

        return ESP_OK;
    }
    else if (strcmp(action->valuestring, "ap_scan_result") == 0)
    {
        uint16_t count = 0;
        wifi_ap_record_t *list = NULL;

        esp_err_t err = dw_station_scan_result(&count, &list);
        if (err != ESP_OK)
        {
            send_response_json("response", "wifi", "ap_scan_error", "scan data unavailable", false);
            return err;
        }

        cJSON *data_obj = cJSON_CreateObject();
        if (!data_obj)
        {
            ESP_LOGE(TAG, "Failed to create data object");
            dw_free_scan_result(list);
            return ESP_ERR_NO_MEM;
        }

        cJSON *networks_array = cJSON_CreateArray();
        if (!networks_array)
        {
    
            dw_free_scan_result(list);
            return ESP_ERR_NO_MEM;
        }

        for (int i = 0; i < count; i++)
        {
            cJSON *ap = cJSON_CreateObject();
            if (!ap)
            {
                cJSON_Delete(networks_array);
        
                dw_free_scan_result(list);
                return ESP_ERR_NO_MEM;
            }

            char ssid[33] = {0};
            strlcpy(ssid, (char *)list[i].ssid, sizeof(ssid));

            cJSON_AddStringToObject(ap, "ssid", ssid);
            cJSON_AddNumberToObject(ap, "rssi", list[i].rssi);
            cJSON_AddNumberToObject(ap, "channel", list[i].primary);

            const char *auth_str = "UNKNOWN";
            switch (list[i].authmode)
            {
            case WIFI_AUTH_OPEN:
                auth_str = "OPEN";
                break;
            case WIFI_AUTH_WEP:
                auth_str = "WEP";
                break;
            case WIFI_AUTH_WPA_PSK:
                auth_str = "WPA";
                break;
            case WIFI_AUTH_WPA2_PSK:
                auth_str = "WPA2";
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                auth_str = "WPA/WPA2";
                break;
            case WIFI_AUTH_WPA2_ENTERPRISE:
                auth_str = "WPA2-Enterprise";
                break;
            case WIFI_AUTH_WPA3_PSK:
                auth_str = "WPA3";
                break;
            case WIFI_AUTH_WPA2_WPA3_PSK:
                auth_str = "WPA2/WPA3";
                break;
            default:
                auth_str = "UNKNOWN";
                break;
            }
            cJSON_AddStringToObject(ap, "authmode", auth_str);

            cJSON_AddItemToArray(networks_array, ap);
        }

        send_response_json("response", "wifi", "ap_scan_result", data_obj, true);

        dw_free_scan_result(list);
        return ESP_OK;
    }
    else if (strcmp(action->valuestring, "ap_connect") == 0)
    {
        char standalone[8] = {0};
        size_t str_size = sizeof(standalone);
        esp_err_t err = nvs_load_data("wifi", "standalone", standalone, &str_size, NVS_TYPE_STR);

        if (err == ESP_OK && strcmp(standalone, "true") == 0)
        {
            send_response_json("response", "wifi", "ap_connect_error", "standalone mode active", false);
            return ESP_OK;
        }

        cJSON *data_obj = cJSON_GetObjectItemCaseSensitive(json, "data");
        if (!cJSON_IsObject(data_obj))
        {
            send_response_json("response", "wifi", "ap_connect_error", "missing or invalid 'data'", false);
            return ESP_OK;
        }

        // Проверка длины всех строковых полей в data_obj
        cJSON *current_item = data_obj->child;
        while (current_item != NULL)
        {
            if (cJSON_IsString(current_item) && current_item->valuestring != NULL)
            {
                size_t str_len = strlen(current_item->valuestring);
                if (str_len >= 32) // 32 символа, включая null-терминатор
                {
                    ESP_LOGE(TAG, "String field '%s' too long: %zu characters (max 31)", current_item->string, str_len);
                    send_response_json("response", "wifi", "ap_connect_error", "string field too long (max 31 characters)", false);
                    return ESP_OK;
                }
            }
            current_item = current_item->next;
        }

        cJSON *ssid = cJSON_GetObjectItemCaseSensitive(data_obj, "ssid");
        cJSON *password = cJSON_GetObjectItemCaseSensitive(data_obj, "password");
        cJSON *authmode = cJSON_GetObjectItemCaseSensitive(data_obj, "authmode");

        if (!cJSON_IsString(ssid) || ssid->valuestring == NULL)
        {
            send_response_json("response", "wifi", "ap_connect_error", "ssid is required", false);
            return ESP_OK;
        }

        size_t ssid_len = strlen(ssid->valuestring);
        if (ssid_len >= 32)
        {
            send_response_json("response", "wifi", "ap_connect_error", "ssid too long", false);
            return ESP_OK;
        }

        wifi_sta_config_t sta_config = {
            .channel = 0,
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold.rssi = -120,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .failure_retry_cnt = 1,
        };

        memcpy(sta_config.ssid, ssid->valuestring, ssid_len);
        sta_config.ssid[ssid_len] = '\0';

        wifi_auth_mode_t auth_mode = WIFI_AUTH_OPEN;

        if (cJSON_IsString(authmode) && authmode->valuestring != NULL)
        {
            // Валидация authmode: проверим, что значение входит в допустимый список
            if (strcmp(authmode->valuestring, "OPEN") != 0 &&
                strcmp(authmode->valuestring, "WEP") != 0 &&
                strcmp(authmode->valuestring, "WPA") != 0 &&
                strcmp(authmode->valuestring, "WPA2") != 0 &&
                strcmp(authmode->valuestring, "WPA/WPA2") != 0 &&
                strcmp(authmode->valuestring, "WPA3") != 0 &&
                strcmp(authmode->valuestring, "WPA2/WPA3") != 0)
            {
                ESP_LOGE(TAG, "Invalid authmode provided: %s", authmode->valuestring);
                send_response_json("response", "wifi", "ap_connect_error", "invalid authmode", false);
                return ESP_OK;
            }

            if (strcmp(authmode->valuestring, "OPEN") == 0)
                auth_mode = WIFI_AUTH_OPEN;
            else if (strcmp(authmode->valuestring, "WEP") == 0)
                auth_mode = WIFI_AUTH_WEP;
            else if (strcmp(authmode->valuestring, "WPA") == 0)
                auth_mode = WIFI_AUTH_WPA_PSK;
            else if (strcmp(authmode->valuestring, "WPA2") == 0)
                auth_mode = WIFI_AUTH_WPA2_PSK;
            else if (strcmp(authmode->valuestring, "WPA/WPA2") == 0)
                auth_mode = WIFI_AUTH_WPA_WPA2_PSK;
            else if (strcmp(authmode->valuestring, "WPA3") == 0)
                auth_mode = WIFI_AUTH_WPA3_PSK;
            else if (strcmp(authmode->valuestring, "WPA2/WPA3") == 0)
                auth_mode = WIFI_AUTH_WPA2_WPA3_PSK;
        }
        else
        {
            // Если authmode не указан, используем OPEN
            auth_mode = WIFI_AUTH_OPEN;
        }

        if (cJSON_IsString(password) && password->valuestring != NULL)
        {
            size_t pass_len = strlen(password->valuestring);
            if (pass_len >= 64) // длина пароля ограничена 64 байтами
            {
                send_response_json("response", "wifi", "ap_connect_error", "password too long", false);
                return ESP_OK;
            }

            memcpy(sta_config.password, password->valuestring, pass_len);
            sta_config.password[pass_len] = '\0';

            sta_config.threshold.authmode = (pass_len > 0) ? auth_mode : WIFI_AUTH_OPEN;
        }
        else
        {
            sta_config.password[0] = '\0';
            sta_config.threshold.authmode = WIFI_AUTH_OPEN;
        }

        ESP_LOGI(TAG, "Connecting to SSID: %s, Auth mode: %d", ssid->valuestring, sta_config.threshold.authmode);

        dwnvs_delete_sta_config();
        err = dw_station_connect_with_auto_reconnect(&sta_config, captive_sta_event_handler, NULL, false);

        if (err == ESP_OK)
        {
            send_response_json("response", "wifi", "ap_connect_ok", NULL, false);
        }
        else
        {
            send_response_json("response", "wifi", "ap_connect_error", esp_err_to_name(err), false);
            ESP_LOGE(TAG, "Station connect error: %s", esp_err_to_name(err));
        }

        return ESP_OK;
    }
    else if (strcmp(action->valuestring, "ap_disconnect") == 0)
    {
        esp_err_t err = dw_station_stop();

        if (err == ESP_OK)
        {
            dw_station_set_auto_reconnect(false);
            xEventGroupSetBits(captive_wifi_group, DISCONNECT_BY_USER);
            send_response_json("response", "wifi", "ap_disconnect_success", NULL, false);
        }
        else
        {
            send_response_json("response", "wifi", "ap_disconnect_error", NULL, false);
            ESP_LOGE(TAG, "Station connect error: %s", esp_err_to_name(err));
        }
    }
    else if (strcmp(action->valuestring, "ap_status") == 0)
    {
        char ssid_str[32] = {0};
        char ip_str[16] = {0};
        char gw_str[16] = {0};
        char nm_str[16] = {0};

        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
        {
            strlcpy(ssid_str, (char *)ap_info.ssid, sizeof(ssid_str));
        }

        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
        {
            esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
            esp_ip4addr_ntoa(&ip_info.gw, gw_str, sizeof(gw_str));
            esp_ip4addr_ntoa(&ip_info.netmask, nm_str, sizeof(nm_str));
        }

        cJSON *data_obj = cJSON_CreateObject();
        if (!data_obj)
        {
            ESP_LOGE(TAG, "Failed to create data object");
            return ESP_ERR_NO_MEM;
        }

        cJSON *conn_obj = cJSON_CreateObject();
        if (conn_obj)
        {
            cJSON_AddStringToObject(conn_obj, "ssid", ssid_str);
            cJSON_AddStringToObject(conn_obj, "ip", ip_str);
            cJSON_AddStringToObject(conn_obj, "gateway", gw_str);
            cJSON_AddStringToObject(conn_obj, "netmask", nm_str);

            esp_err_t eth_status = dw_check_internet_connection();

            if (eth_status == ESP_OK)
            {
                cJSON_AddBoolToObject(conn_obj, "ethernet", true);
            }
            else
            {
                cJSON_AddBoolToObject(conn_obj, "ethernet", false);
            }

            cJSON_AddItemToObject(data_obj, "connect", conn_obj);
        }

        cJSON *version_obj = cJSON_CreateObject();
        if (version_obj)
        {
            const esp_app_desc_t *app = esp_app_get_description();
            const esp_bootloader_desc_t *boot = esp_bootloader_get_description();

            if (app)
            {
                cJSON_AddStringToObject(version_obj, "application", app->version);
            }
            else
            {
                cJSON_AddStringToObject(version_obj, "application", "unknown");
            }

            if (boot)
            {
                char boot_version[16] = {0};
                snprintf(boot_version, sizeof(boot_version), "%lu", boot->version);
                cJSON_AddStringToObject(version_obj, "bootloader", boot_version);
            }
            else
            {
                cJSON_AddStringToObject(version_obj, "bootloader", "unknown");
            }

            cJSON_AddItemToObject(data_obj, "version", version_obj);
        }

        send_response_json("event", "wifi", "ap_status", data_obj, true);

    }
    else if (strcmp(action->valuestring, "ap_config") == 0)
    {
        char standalone[8] = {0};
        size_t str_size = sizeof(standalone);
        esp_err_t err = nvs_load_data("wifi", "standalone", standalone, &str_size, NVS_TYPE_STR);

        if (err != ESP_OK)
        {
            snprintf(standalone, sizeof(standalone), "false");
        }

        wifi_sta_config_t wifi_sta = {0};
        err = dwnvs_load_sta_config(&wifi_sta);

        cJSON *data_obj = cJSON_CreateObject();
        if (!data_obj)
        {
            ESP_LOGE(TAG, "Failed to create data object");
            return ESP_ERR_NO_MEM;
        }

        if (err == ESP_OK)
        {
            cJSON_AddStringToObject(data_obj, "ssid", (char *)wifi_sta.ssid);
            cJSON_AddStringToObject(data_obj, "password", (char *)wifi_sta.password);
            cJSON_AddStringToObject(data_obj, "standalone", standalone);

            const char *auth_str = "UNKNOWN";
            switch (wifi_sta.threshold.authmode)
            {
            case WIFI_AUTH_OPEN:
                auth_str = "OPEN";
                break;
            case WIFI_AUTH_WEP:
                auth_str = "WEP";
                break;
            case WIFI_AUTH_WPA_PSK:
                auth_str = "WPA";
                break;
            case WIFI_AUTH_WPA2_PSK:
                auth_str = "WPA2";
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                auth_str = "WPA/WPA2";
                break;
            case WIFI_AUTH_WPA2_ENTERPRISE:
                auth_str = "WPA2-Enterprise";
                break;
            case WIFI_AUTH_WPA3_PSK:
                auth_str = "WPA3";
                break;
            case WIFI_AUTH_WPA2_WPA3_PSK:
                auth_str = "WPA2/WPA3";
                break;
            default:
                auth_str = "UNKNOWN";
                break;
            }
            cJSON_AddStringToObject(data_obj, "authmode", auth_str);
        }
        else
        {
            cJSON_AddNullToObject(data_obj, "ssid");
            cJSON_AddNullToObject(data_obj, "password");
            cJSON_AddStringToObject(data_obj, "standalone", standalone);
        }

        send_response_json("event", "wifi", "ap_config", data_obj, true);

    }
    else if (strcmp(action->valuestring, "save_partial") == 0)
    {
        cJSON *data_obj = cJSON_GetObjectItemCaseSensitive(json, "data");
        if (data_obj == NULL)
        {
            ESP_LOGW(TAG, "Missing 'data' in save request");
            send_response_json("response", "wifi", "error_partial", "\"missing 'data'\"", false);
            return ESP_ERR_INVALID_ARG;
        }

        esp_err_t result = parse_and_save_json_settings(
            "wifi",
            data_obj,
            wifi_params,
            sizeof(wifi_params) / sizeof(wifi_params[0]));

        if (result == ESP_OK)
        {
            send_response_json("response", "wifi", "saved_partial", NULL, false);
        }
        else
        {
            ESP_LOGE(TAG, "Save failed: %s", esp_err_to_name(result));
            send_response_json("response", "wifi", "error_partial", "save failed", false);
        }
    }
    else
    {
        send_response_json("response", "wifi", "common_error", "unknown action", false);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}
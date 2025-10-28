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

    cJSON *response = cJSON_CreateObject();
    if (!response)
    {
        ESP_LOGE(TAG, "Failed to create response object");
        goto cleanup;
    }

    cJSON_AddStringToObject(response, "type", "event");
    cJSON_AddStringToObject(response, "target", "wifi");
    cJSON_AddStringToObject(response, "status", "scan_ap_completed");

    cJSON *data_obj = cJSON_CreateObject();
    if (!data_obj)
    {
        ESP_LOGE(TAG, "Failed to create data object");
        cJSON_Delete(response);
        goto cleanup;
    }

    cJSON_AddNumberToObject(data_obj, "count", ap_count);
    cJSON_AddItemToObject(response, "data", data_obj);

    char *json_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    if (!json_str)
    {
        ESP_LOGE(TAG, "Failed to print JSON response");
        goto cleanup;
    }

    ESP_LOGD(TAG, "Sending: %s", json_str);

    if (!ws_server_send_string(json_str))
    {
        ESP_LOGE(TAG, "Failed to send scan result to client");
    }

    cJSON_free(json_str);

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

        cJSON *response = cJSON_CreateObject();
        if (response)
        {
            cJSON_AddStringToObject(response, "type", "event");
            cJSON_AddStringToObject(response, "target", "wifi");
            cJSON_AddStringToObject(response, "status", "connection_ap_got_ip");

            cJSON *data_obj = cJSON_CreateObject();
            if (data_obj)
            {
                cJSON_AddStringToObject(data_obj, "ssid", ssid_str);
                cJSON_AddStringToObject(data_obj, "ip", ip_str);
                cJSON_AddStringToObject(data_obj, "gateway", gw_str);
                cJSON_AddStringToObject(data_obj, "netmask", nm_str);
            }

            cJSON_AddItemToObject(response, "data", data_obj);

            char *json_str = cJSON_PrintUnformatted(response);
            cJSON_Delete(response);

            if (json_str)
            {
                ws_server_send_string(json_str);
                cJSON_free(json_str);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to serialize JSON for got_ip event");
            }

            disconnect_notify = false;
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
        cJSON *response = cJSON_CreateObject();
        if (response)
        {
            cJSON_AddStringToObject(response, "type", "event");
            cJSON_AddStringToObject(response, "target", "wifi");
            cJSON_AddStringToObject(response, "status", "connection_ap_success");

            cJSON *data_obj = cJSON_CreateObject();
            if (data_obj)
            {
            }

            cJSON_AddItemToObject(response, "data", data_obj);

            char *json_str = cJSON_PrintUnformatted(response);
            cJSON_Delete(response);

            if (json_str)
            {
                ws_server_send_string(json_str);
                cJSON_free(json_str);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to serialize JSON for connected event");
            }
        }
        else
        {
            ESP_LOGE(TAG, "Failed to create JSON response for connected event");
            return;
        }
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

        cJSON *response = cJSON_CreateObject();
        if (response)
        {
            cJSON_AddStringToObject(response, "type", "event");
            cJSON_AddStringToObject(response, "target", "wifi");
            cJSON_AddStringToObject(response, "status", "disconnected_ap_from_reason");

            cJSON *data_obj = cJSON_CreateObject();
            if (data_obj)
            {
                cJSON_AddNumberToObject(data_obj, "reason", disconn->reason);
                cJSON_AddStringToObject(data_obj, "reason_str", wifi_reason_to_string(disconn->reason));
            }

            cJSON_AddItemToObject(response, "data", data_obj);

            char *json_str = cJSON_PrintUnformatted(response);
            cJSON_Delete(response);

            if (json_str)
            {
                ws_server_send_string(json_str);
                cJSON_free(json_str);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to serialize JSON for disconnected event");
            }
        }
        else
        {
            ESP_LOGE(TAG, "Failed to create JSON response for disconnected event");
            return;
        }
    }
}
//=================================================================
esp_err_t wifi_module_target(cJSON *json)
{
    cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
    if (!cJSON_IsString(action) || action->valuestring == NULL)
    {
        send_response_json("response", "wifi", "connect_ap_failed", "missing or invalid 'action'");
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

    if (strcmp(action->valuestring, "scan_ap_start") == 0)
    {
        esp_err_t scan_err = dw_station_scan_start(wifi_scan_done_handler, NULL);

        if (scan_err == ESP_OK)
        {
            send_response_json("response", "wifi", "scan_ap_started", NULL);
        }
        else if (scan_err == ESP_ERR_INVALID_STATE)
        {
            send_response_json("response", "wifi", "scan_ap_already_started", NULL);
        }
        else
        {
            send_response_json("response", "wifi", "scan_ap_failed", esp_err_to_name(scan_err));
        }

        return ESP_OK;
    }
    else if (strcmp(action->valuestring, "scan_ap_results") == 0)
    {
        uint16_t count = 0;
        wifi_ap_record_t *list = NULL;

        esp_err_t err = dw_station_scan_result(&count, &list);
        if (err != ESP_OK)
        {
            send_response_json("response", "wifi", "scan_ap_error", "scan data unavailable");
            return err;
        }

        cJSON *response = cJSON_CreateObject();
        if (!response)
        {
            ESP_LOGE(TAG, "Failed to create response object");
            dw_free_scan_result(list);
            return ESP_ERR_NO_MEM;
        }

        cJSON_AddStringToObject(response, "type", "response");
        cJSON_AddStringToObject(response, "target", "wifi");
        cJSON_AddStringToObject(response, "status", "scan_ap_results");

        cJSON *data_obj = cJSON_CreateObject();
        if (!data_obj)
        {
            ESP_LOGE(TAG, "Failed to create data object");
            cJSON_Delete(response);
            dw_free_scan_result(list);
            return ESP_ERR_NO_MEM;
        }

        cJSON *networks_array = cJSON_CreateArray();
        if (!networks_array)
        {
            cJSON_Delete(data_obj);
            cJSON_Delete(response);
            dw_free_scan_result(list);
            return ESP_ERR_NO_MEM;
        }

        for (int i = 0; i < count; i++)
        {
            cJSON *ap = cJSON_CreateObject();
            if (!ap)
            {
                cJSON_Delete(networks_array);
                cJSON_Delete(data_obj);
                cJSON_Delete(response);
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

        cJSON_AddItemToObject(data_obj, "networks", networks_array);
        cJSON_AddItemToObject(response, "data", data_obj);

        char *json_str = cJSON_PrintUnformatted(response);
        cJSON_Delete(response);

        if (json_str)
        {
            ws_server_send_string(json_str);
            cJSON_free(json_str);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to generate JSON for scan results");
        }

        dw_free_scan_result(list);
        return ESP_OK;
    }
    else if (strcmp(action->valuestring, "connect_ap_attempt") == 0)
    {
        char standalone[8] = {0};
        size_t str_size = sizeof(standalone);
        esp_err_t err = nvs_load_data("wifi", "standalone", standalone, &str_size, NVS_TYPE_STR);

        if (err == ESP_OK && strcmp(standalone, "true") == 0)
        {
            send_response_json("response", "wifi", "connect_ap_failed", "standalone mode active");
            return ESP_OK;
        }

        cJSON *data_obj = cJSON_GetObjectItemCaseSensitive(json, "data");
        if (!cJSON_IsObject(data_obj))
        {
            send_response_json("response", "wifi", "connect_ap_failed", "missing or invalid 'data'");
            return ESP_OK; // Не возвращаем ошибку, т.к. это ошибка клиента
        }

        cJSON *ssid = cJSON_GetObjectItemCaseSensitive(data_obj, "ssid");
        cJSON *password = cJSON_GetObjectItemCaseSensitive(data_obj, "password");
        cJSON *authmode = cJSON_GetObjectItemCaseSensitive(data_obj, "authmode");

        if (!cJSON_IsString(ssid) || ssid->valuestring == NULL)
        {
            send_response_json("response", "wifi", "connect_ap_failed", "ssid is required");
            return ESP_OK;
        }

        size_t ssid_len = strlen(ssid->valuestring);
        if (ssid_len >= 32)
        {
            send_response_json("response", "wifi", "connect_ap_failed", "ssid too long");
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
            else
            {
                ESP_LOGW(TAG, "Unknown authmode '%s', defaulting to OPEN", authmode->valuestring);
                auth_mode = WIFI_AUTH_OPEN;
            }
        }

        if (cJSON_IsString(password) && password->valuestring != NULL)
        {
            size_t pass_len = strlen(password->valuestring);
            if (pass_len >= 64) // длина пароля ограничена 64 байтами
            {
                send_response_json("response", "wifi", "connect_ap_failed", "password too long");
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
            send_response_json("response", "wifi", "connect_ap_trying", NULL);
        }
        else
        {
            send_response_json("response", "wifi", "connect_ap_failed", esp_err_to_name(err));
            ESP_LOGE(TAG, "Station connect error: %s", esp_err_to_name(err));
        }

        return ESP_OK;
    }
    else if (strcmp(action->valuestring, "disconnect_ap_attempt") == 0)
    {
        esp_err_t err = dw_station_stop();

        if (err == ESP_OK)
        {
            dw_station_set_auto_reconnect(false);
            xEventGroupSetBits(captive_wifi_group, DISCONNECT_BY_USER);
            send_response_json("response", "wifi", "disconnect_ap_success", NULL);
        }
        else
        {
            send_response_json("response", "wifi", "disconnect_ap_failed", NULL);
            ESP_LOGE(TAG, "Station connect error: %s", esp_err_to_name(err));
        }
    }
    else if (strcmp(action->valuestring, "load_connection_status") == 0)
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

        cJSON *response = cJSON_CreateObject();
        if (!response)
        {
            ESP_LOGE(TAG, "Failed to create response object");
            send_response_json("response", "wifi", "load_connection_status_failed", "memory allocation failed");
            return ESP_ERR_NO_MEM;
        }

        cJSON_AddStringToObject(response, "type", "response");
        cJSON_AddStringToObject(response, "target", "wifi");
        cJSON_AddStringToObject(response, "status", "load_connection_status");

        // Создаем объект данных
        cJSON *data_obj = cJSON_CreateObject();
        if (data_obj)
        {
            // Создаем и заполняем объект подключения
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

            // Создаем и заполняем объект версии
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

            cJSON_AddItemToObject(response, "data", data_obj);
        }

        // Генерируем JSON строку
        char *json_str = cJSON_PrintUnformatted(response);
        if (json_str)
        {
            ws_server_send_string(json_str);
            cJSON_free(json_str); // cJSON_PrintUnformatted выделяет память через cJSON_malloc
        }
        else
        {
            ESP_LOGE(TAG, "Failed to generate JSON string");
            send_response_json("response", "wifi", "load_connection_status_failed", "json generation failed");
        }

        // Освобождаем основной cJSON объект
        cJSON_Delete(response);
    }
    else if (strcmp(action->valuestring, "load_ap_config") == 0)
    {
        cJSON *response = cJSON_CreateObject();
        if (!response)
        {
            ESP_LOGE(TAG, "Failed to create response object");
            return ESP_ERR_NO_MEM;
        }

        cJSON_AddStringToObject(response, "type", "response");
        cJSON_AddStringToObject(response, "target", "wifi");

        char standalone[8] = {0};
        size_t str_size = sizeof(standalone);
        esp_err_t err = nvs_load_data("wifi", "standalone", standalone, &str_size, NVS_TYPE_STR);

        if (err != ESP_OK)
        {
            snprintf(standalone, sizeof(standalone), "false");
        }

        wifi_sta_config_t wifi_sta = {0};
        err = dwnvs_load_sta_config(&wifi_sta);

        cJSON_AddStringToObject(response, "status", "load_ap_config");
        cJSON *data_obj = cJSON_CreateObject();

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

        cJSON_AddItemToObject(response, "data", data_obj);

        char *json_str = cJSON_PrintUnformatted(response);
        cJSON_Delete(response);

        if (json_str)
        {
            ws_server_send_string(json_str);
            cJSON_free(json_str);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to generate JSON for scan results");
        }

        return ESP_OK;
    }
    else if (strcmp(action->valuestring, "save_partial") == 0)
    {
        cJSON *data_obj = cJSON_GetObjectItemCaseSensitive(json, "data");
        if (data_obj == NULL)
        {
            ESP_LOGW(TAG, "Missing 'data' in save request");
            send_response_json("response", "wifi", "error_partial", "\"missing 'data'\"");
            return ESP_ERR_INVALID_ARG;
        }

        esp_err_t result = parse_and_save_json_settings(
            "wifi",
            data_obj,
            wifi_params,
            sizeof(wifi_params) / sizeof(wifi_params[0]));

        if (result == ESP_OK)
        {
            send_response_json("response", "wifi", "saved_partial", NULL);
        }
        else
        {
            ESP_LOGE(TAG, "Save failed: %s", esp_err_to_name(result));
            send_response_json("response", "wifi", "error_partial", "save failed");
        }
    }
    else
    {
        send_response_json("response", "wifi", "error", "unknown action");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}
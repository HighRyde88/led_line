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
#include "data_parser.h"
#include "modules.h"

#include <esp_netif.h>
#include <arpa/inet.h>

static const char *TAG = "network module";
//=================================================================
esp_err_t network_module_target(cJSON *json)
{
    cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
    if (!cJSON_IsString(action) || action->valuestring == NULL)
    {
        ESP_LOGW(TAG, "Invalid or missing 'action'");
        send_response_json("response", "network", "error_partial", "\"invalid or missing 'action'\"");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ESP_OK;

    if (strcmp(action->valuestring, "save_partial") == 0)
    {
        cJSON *data_obj = cJSON_GetObjectItemCaseSensitive(json, "data");
        if (data_obj == NULL)
        {
            ESP_LOGW(TAG, "Missing 'data' in save request");
            send_response_json("response", "network", "error_partial", "missing 'data'");
            return ESP_ERR_INVALID_ARG;
        }

        cJSON *mode_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "mode");

        if (!cJSON_IsString(mode_obj) || mode_obj->valuestring == NULL)
        {
            ESP_LOGW(TAG, "Missing 'mode' in save request");
            send_response_json("response", "network", "error_partial", "missing 'mode'");
            return ESP_ERR_INVALID_ARG;
        }

        esp_err_t mode_result = ESP_FAIL;
        esp_err_t data_result = ESP_FAIL;

        if (strcmp(mode_obj->valuestring, "dhcp") == 0)
        {
            data_result = dwnvs_delete_ipinfo("network");
            mode_result = ESP_OK;
            data_result = ESP_OK;
        }
        else if (strcmp(mode_obj->valuestring, "static") == 0)
        {
            esp_netif_ip_info_t ip_info = {0};

            cJSON *ip_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "ip");
            cJSON *netmask_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "netmask");
            cJSON *gateway_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "gateway");

            // Проверим длину строк IP-адресов
            if (ip_obj != NULL && cJSON_IsString(ip_obj) && ip_obj->valuestring != NULL)
            {
                size_t ip_len = strlen(ip_obj->valuestring);
                if (ip_len > 15)
                {
                    ESP_LOGE(TAG, "IP address too long: %s", ip_obj->valuestring);
                    send_response_json("response", "network", "error_partial", "IP address too long (max 15 chars)");
                    return ESP_ERR_INVALID_ARG;
                }
            }

            if (netmask_obj != NULL && cJSON_IsString(netmask_obj) && netmask_obj->valuestring != NULL)
            {
                size_t netmask_len = strlen(netmask_obj->valuestring);
                if (netmask_len > 15)
                {
                    ESP_LOGE(TAG, "Netmask too long: %s", netmask_obj->valuestring);
                    send_response_json("response", "network", "error_partial", "Netmask too long (max 15 chars)");
                    return ESP_ERR_INVALID_ARG;
                }
            }

            if (gateway_obj != NULL && cJSON_IsString(gateway_obj) && gateway_obj->valuestring != NULL)
            {
                size_t gateway_len = strlen(gateway_obj->valuestring);
                if (gateway_len > 15)
                {
                    ESP_LOGE(TAG, "Gateway too long: %s", gateway_obj->valuestring);
                    send_response_json("response", "network", "error_partial", "Gateway too long (max 15 chars)");
                    return ESP_ERR_INVALID_ARG;
                }
            }

            if ((ip_obj != NULL && cJSON_IsString(ip_obj) && ip_obj->valuestring != NULL) &&
                (netmask_obj != NULL && cJSON_IsString(netmask_obj) && netmask_obj->valuestring != NULL) &&
                (gateway_obj != NULL && cJSON_IsString(gateway_obj) && gateway_obj->valuestring != NULL))
            {
                if (inet_aton(ip_obj->valuestring, &ip_info.ip) &&
                    inet_aton(netmask_obj->valuestring, &ip_info.netmask) &&
                    inet_aton(gateway_obj->valuestring, &ip_info.gw))
                {
                    data_result = dwnvs_save_ipinfo("network", &ip_info);
                }
                else
                {
                    data_result = ESP_FAIL;
                }
            }
            else
            {
                data_result = ESP_FAIL;
            }
            mode_result = ESP_OK;
        }
        else
        {
            ESP_LOGW(TAG, "Unknown 'mode' in save request");
            send_response_json("response", "network", "error_partial", "Unknown 'mode'");
            return ESP_ERR_INVALID_ARG;
        }

        if (mode_result == ESP_OK && data_result == ESP_OK)
        {
            send_response_json("response", "network", "saved_partial", NULL);
            result = ESP_OK;
        }
        else
        {
            ESP_LOGE(TAG, "Save failed: mode_result=%s, data_result=%s",
                     esp_err_to_name(mode_result), esp_err_to_name(data_result));
            send_response_json("response", "network", "error_partial", "save failed");
            result = ESP_FAIL;
        }
    }
    else if (strcmp(action->valuestring, "load_partial") == 0)
    {
        cJSON *response = cJSON_CreateObject();
        if (!response)
        {
            ESP_LOGE(TAG, "Failed to create JSON response");
            return ESP_ERR_NO_MEM;
        }

        cJSON_AddStringToObject(response, "type", "response");
        cJSON_AddStringToObject(response, "target", "network");
        cJSON_AddStringToObject(response, "status", "load_partial");

        cJSON *data_obj = cJSON_CreateObject();

        esp_netif_ip_info_t ip_info = {0};

        char ip_str[IP4ADDR_STRLEN_MAX];
        char gw_str[IP4ADDR_STRLEN_MAX];
        char nm_str[IP4ADDR_STRLEN_MAX];

        if (dwnvs_load_ipinfo("network", &ip_info) == ESP_OK)
        {
            ip4addr_ntoa_r((const ip4_addr_t *)&ip_info.ip, ip_str, IP4ADDR_STRLEN_MAX);
            ip4addr_ntoa_r((const ip4_addr_t *)&ip_info.gw, gw_str, IP4ADDR_STRLEN_MAX);
            ip4addr_ntoa_r((const ip4_addr_t *)&ip_info.netmask, nm_str, IP4ADDR_STRLEN_MAX);

            cJSON_AddStringToObject(data_obj, "mode", "static");
            cJSON_AddStringToObject(data_obj, "ip", ip_str);
            cJSON_AddStringToObject(data_obj, "gateway", gw_str);
            cJSON_AddStringToObject(data_obj, "netmask", nm_str);
        }
        else
        {
            cJSON_AddStringToObject(data_obj, "mode", "dhcp");
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
            ESP_LOGE(TAG, "Failed to serialize JSON response");
        }
    }
    else
    {
        ESP_LOGW(TAG, "Unknown action: %s", action->valuestring);
        send_response_json("response", "network", "error_partial", "\"unknown action\"");
        return ESP_ERR_INVALID_ARG;
    }

    return result;
}
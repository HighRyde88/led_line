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

static const char *TAG = "device module";

static const config_param_t device_params[] = {
    {"hostname", "hostname"},
};
//=================================================================
esp_err_t device_module_target(cJSON *json)
{
    cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
    if (!cJSON_IsString(action) || action->valuestring == NULL)
    {
        ESP_LOGW(TAG, "Invalid or missing 'action'");
        send_response_json("response", "device", "error_partial", "\"invalid or missing 'action'\"");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ESP_OK;
    if (strcmp(action->valuestring, "save_partial") == 0)
    {
        cJSON *data_obj = cJSON_GetObjectItemCaseSensitive(json, "data");
        if (data_obj == NULL)
        {
            ESP_LOGW(TAG, "Missing 'data' in save request");
            send_response_json("response", "device", "error_partial", "\"missing 'data'\"");
            return ESP_ERR_INVALID_ARG;
        }

        // Проверим hostname на валидность длины
        cJSON *hostname_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "hostname");
        if (cJSON_IsString(hostname_obj) && hostname_obj->valuestring != NULL)
        {
            size_t hostname_len = strlen(hostname_obj->valuestring);
            if (hostname_len > 63) // ESP-IDF обычно ограничивает hostname до 63 байт
            {
                ESP_LOGE(TAG, "Hostname too long: %zu characters (max 63)", hostname_len);
                send_response_json("response", "device", "error_partial", "\"Hostname too long (max 63 characters)\"");
                return ESP_ERR_INVALID_ARG;
            }
        }

        esp_err_t result = parse_and_save_json_settings(
            "device",
            data_obj,
            device_params,
            sizeof(device_params) / sizeof(device_params[0]));

        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to parse and save general settings");
            send_response_json("response", "device", "error_partial", "parse and save failed");
            return result;
        }

        if (cJSON_IsString(hostname_obj) && hostname_obj->valuestring != NULL)
        {
            dw_set_hostname_to_netif(WIFI_IF_STA, hostname_obj->valuestring);
        }
        else
        {
            ESP_LOGI(TAG, "Hostname not provided, skipping hostname update");
            result = ESP_OK;
        }

        if (result == ESP_OK)
        {
            send_response_json("response", "device", "saved_partial", NULL);
        }
        else
        {
            ESP_LOGE(TAG, "Save failed: %s", esp_err_to_name(result));
            send_response_json("response", "device", "error_partial", "save failed");
        }

        return result;
    }
    else if (strcmp(action->valuestring, "load_partial") == 0)
    {
        cJSON *load_settings = load_and_parse_json_settings(
            "device",
            device_params,
            sizeof(device_params) / sizeof(device_params[0]));

        if (load_settings != NULL)
        {
            cJSON *response = cJSON_CreateObject();
            if (!response)
            {
                ESP_LOGE(TAG, "Failed to create JSON response");
                cJSON_Delete(load_settings);
                return ESP_ERR_NO_MEM;
            }

            cJSON_AddStringToObject(response, "type", "response");
            cJSON_AddStringToObject(response, "target", "device");
            cJSON_AddStringToObject(response, "status", "load_partial");
            cJSON_AddItemToObject(response, "data", load_settings);

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
            ESP_LOGE(TAG, "Load failed");
            send_response_json("response", "device", "error_partial", "load failed");
            return ESP_ERR_NOT_FOUND;
        }
    }
    else
    {
        ESP_LOGW(TAG, "Unknown action: %s", action->valuestring);
        send_response_json("response", "device", "error_partial", "unknown action");
        return ESP_ERR_INVALID_ARG;
    }

    return result;
}
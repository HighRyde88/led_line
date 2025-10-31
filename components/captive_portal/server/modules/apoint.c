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

static const char *TAG = "apoint module";
//=================================================================
esp_err_t apoint_module_target(cJSON *json)
{
    cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
    if (!cJSON_IsString(action) || action->valuestring == NULL)
    {
        ESP_LOGW(TAG, "Invalid or missing 'action'");
        send_response_json("response", "apoint", "error_partial", "\"invalid or missing 'action'\"", false);
        return ESP_ERR_INVALID_ARG;
    }

    wifi_ap_config_t ap_config = {0};
    esp_err_t result = ESP_OK;

    if (strcmp(action->valuestring, "save_partial") == 0)
    {
        cJSON *data_obj = cJSON_GetObjectItemCaseSensitive(json, "data");
        if (data_obj == NULL)
        {
            ESP_LOGW(TAG, "Missing 'data' in save request");
            send_response_json("response", "apoint", "error_partial", "\"missing 'data'\"", false);
            return ESP_ERR_INVALID_ARG;
        }

        cJSON *ssid_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "ssid");
        cJSON *passw_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "password");

        if (cJSON_IsString(ssid_obj) && ssid_obj->valuestring != NULL)
        {
            size_t ssid_len = strlen(ssid_obj->valuestring);
            if (ssid_len > 31) // 32 байта, включая null-терминатор
            {
                ESP_LOGE(TAG, "SSID too long: %zu characters (max 31)", ssid_len);
                send_response_json("response", "apoint", "error_partial", "\"SSID too long (max 31 characters)\"", false);
                return ESP_ERR_INVALID_ARG;
            }
            strncpy((char *)ap_config.ssid, ssid_obj->valuestring, sizeof(ap_config.ssid) - 1);
            ap_config.ssid[sizeof(ap_config.ssid) - 1] = '\0';
        }
        else
        {
            ESP_LOGW(TAG, "SSID is missing or not a string");
            send_response_json("response", "apoint", "error_partial", "\"SSID is missing or invalid\"", false);
            return ESP_ERR_INVALID_ARG;
        }

        if (cJSON_IsString(passw_obj) && passw_obj->valuestring != NULL)
        {
            size_t passw_len = strlen(passw_obj->valuestring);
            if (passw_len > 63) // 64 байта, включая null-терминатор
            {
                ESP_LOGE(TAG, "Password too long: %zu characters (max 63)", passw_len);
                send_response_json("response", "apoint", "error_partial", "\"Password too long (max 63 characters)\"", false);
                return ESP_ERR_INVALID_ARG;
            }
            strncpy((char *)ap_config.password, passw_obj->valuestring, sizeof(ap_config.password) - 1);
            ap_config.password[sizeof(ap_config.password) - 1] = '\0';
        }
        else
        {
            ap_config.password[0] = '\0';
            ESP_LOGI(TAG, "Password not provided, setting to empty");
        }

        result = dwnvs_save_ap_config(&ap_config);

        if (result == ESP_OK)
        {
            send_response_json("response", "apoint", "saved_partial", NULL, false);
        }
        else
        {
            ESP_LOGE(TAG, "Save failed: %s", esp_err_to_name(result));
            send_response_json("response", "apoint", "error_partial", "save failed", false);
        }

        return result;
    }
    else if (strcmp(action->valuestring, "load_partial") == 0)
    {
        cJSON *data_obj = cJSON_CreateObject();
        if (!data_obj)
        {
            ESP_LOGE(TAG, "Failed to create JSON response");
            return ESP_ERR_NO_MEM;
        }

        if (dwnvs_load_ap_config(&ap_config) == ESP_OK && ap_config.ssid[0] != '\0')
        {
            cJSON_AddStringToObject(data_obj, "ssid", (char *)ap_config.ssid);
            cJSON_AddStringToObject(data_obj, "password", (char *)ap_config.password);
        }

        send_response_json("response", "apoint", "load_partial", data_obj, true);
    }
    else
    {
        ESP_LOGW(TAG, "Unknown action: %s", action->valuestring);
        send_response_json("response", "apoint", "error_partial", "unknown action", false);
        return ESP_ERR_INVALID_ARG;
    }

    return result;
}
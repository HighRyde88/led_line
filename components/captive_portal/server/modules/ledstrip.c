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

static const char *TAG = "ledstrip module";

static const config_param_t ledstrip_params[] = {
    {"lednum", "lednum"},
    {"hostname", "hostname"},
    {"ledpin", "ledpin"}};
//=================================================================
esp_err_t ledstrip_module_target(cJSON *json)
{
    cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
    if (!cJSON_IsString(action) || action->valuestring == NULL)
    {
        send_response_json("response", "ledstrip", "error_partial", "missing or invalid 'action'");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ESP_OK;

    if (strcmp(action->valuestring, "save_partial") == 0)
    {
        result = parse_and_save_json_settings("ledstrip", cJSON_GetObjectItemCaseSensitive(json, "data"), ledstrip_params, sizeof(ledstrip_params) / sizeof(ledstrip_params[0]));

        if (result == ESP_OK)
        {
            send_response_json("response", "ledstrip", "saved_partial", NULL);
        }
    }
    else if (strcmp(action->valuestring, "load_partial") == 0)
    {

        cJSON *load_settings = load_and_parse_json_settings(
            "ledstrip",
            ledstrip_params,
            sizeof(ledstrip_params) / sizeof(ledstrip_params[0]));

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
            cJSON_AddStringToObject(response, "target", "ledstrip");
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
            send_response_json("response", "ledstrip", "error_partial", "load failed");
            return ESP_ERR_NOT_FOUND;
        }
    }
    else
    {
        send_response_json("response", "ledstrip", "error_partial", "unknown action");
        return ESP_ERR_INVALID_ARG;
    }

    return result;
}
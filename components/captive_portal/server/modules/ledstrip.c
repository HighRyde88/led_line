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
#include "driver/gpio.h"

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
        cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");

        // Проверим, есть ли в data поле ledpin
        cJSON *ledpin_item = cJSON_GetObjectItemCaseSensitive(data, "ledpin");
        if (ledpin_item != NULL && cJSON_IsString(ledpin_item))
        {
            int temp_ledpin = atoi(ledpin_item->valuestring);
            if (!GPIO_IS_VALID_OUTPUT_GPIO(temp_ledpin))
            {
                ESP_LOGE(TAG, "Invalid LED pin provided: %s", ledpin_item->valuestring);
                send_response_json("response", "ledstrip", "error_partial", "invalid ledpin provided");
                return ESP_ERR_INVALID_ARG;
            }
        }

        // Проверим, есть ли в data поле lednum
        cJSON *lednum_item = cJSON_GetObjectItemCaseSensitive(data, "lednum");
        if (lednum_item != NULL && cJSON_IsString(lednum_item))
        {
            int temp_lednum = atoi(lednum_item->valuestring);
            if (temp_lednum <= 0 || temp_lednum > 512)
            {
                ESP_LOGE(TAG, "Invalid LED count provided: %s (must be 1-%d)", lednum_item->valuestring, 512);
                send_response_json("response", "ledstrip", "error_partial", "invalid lednum provided (must be 1-512)");
                return ESP_ERR_INVALID_ARG;
            }
        }

        result = parse_and_save_json_settings("ledstrip", data, ledstrip_params, sizeof(ledstrip_params) / sizeof(ledstrip_params[0]));

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
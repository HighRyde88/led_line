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
#include "esp_app_desc.h"
#include "esp_bootloader_desc.h"

static const char *TAG = "update module";
//=================================================================
static void ota_update_process(void *pvParameters) {}
//=================================================================
esp_err_t update_module_target(cJSON *json)
{
    cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
    if (!cJSON_IsString(action) || action->valuestring == NULL)
    {
        ESP_LOGW(TAG, "Invalid or missing 'action'");
        send_response_json("response", "update", "error_update", "\"invalid or missing 'action'\"");
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(action->valuestring, "start_update") == 0)
    {
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
        cJSON_AddStringToObject(response, "target", "update");
        cJSON_AddStringToObject(response, "status", "load_partial");

        cJSON *version_obj = cJSON_CreateObject();

        const esp_app_desc_t *app = esp_app_get_description();
        const esp_bootloader_desc_t *boot = esp_bootloader_get_description();

        cJSON_AddStringToObject(version_obj, "application", app->version);
        char boot_version[8] = {0};
        snprintf(boot_version, sizeof(boot_version), "%lu", boot->version);
        cJSON_AddStringToObject(version_obj, "bootloader", boot_version);

        cJSON_AddItemToObject(response, "data", version_obj);

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

    return ESP_OK;
}
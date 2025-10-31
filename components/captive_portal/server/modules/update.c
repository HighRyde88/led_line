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
        send_response_json("response", "update", "error_update", "\"invalid or missing 'action'\"", false);
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(action->valuestring, "start_update") == 0)
    {
    }
    else if (strcmp(action->valuestring, "load_partial") == 0)
    {
        cJSON *data_obj = cJSON_CreateObject();
        if (!data_obj)
        {
            ESP_LOGE(TAG, "Failed to create data object");
            return ESP_ERR_NO_MEM;
        }

        const esp_app_desc_t *app = esp_app_get_description();
        const esp_bootloader_desc_t *boot = esp_bootloader_get_description();

        cJSON_AddStringToObject(data_obj, "application", app->version);
        char boot_version[8] = {0};
        snprintf(boot_version, sizeof(boot_version), "%lu", boot->version);
        cJSON_AddStringToObject(data_obj, "bootloader", boot_version);

        send_response_json("response", "update", "load_partial", data_obj, true);
    }

    return ESP_OK;
}
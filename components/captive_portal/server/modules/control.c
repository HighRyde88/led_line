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

static const char *TAG = "CONFIG";
//=================================================================
static void task_reboot_system(void *pvParameters)
{
    vTaskDelay(500 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Stopping captive portal components...");
    portal_stop(true);

    ESP_LOGI(TAG, "Captive portal stop complete. Restarting device...");
    vTaskDelay(2500 / portTICK_PERIOD_MS);

    nvs_set_flag("is_reboot", true);
    esp_restart();
}

//=================================================================
static void action_task(void *argument)
{
    char *action = (char *)argument;

    if (strcmp(action, "logout") == 0)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Stopping captive portal components...");
        portal_stop(false);
    }
    else if (strcmp(action, "reset") == 0)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "Reset command received. Initiating factory reset...");
        ESP_LOGI(TAG, "Stopping captive portal components...");
        portal_stop(true);

        ESP_LOGI(TAG, "Deleting Wi-Fi configuration from NVS...");
        dwnvs_delete_ap_config();
        dwnvs_delete_sta_config();
        dwnvs_delete_ipinfo("network");

        ESP_LOGI(TAG, "Clearing configuration namespace in NVS...");
        nvs_clear_namespace("device");
        nvs_clear_namespace("ledstrip");
        nvs_clear_namespace("apoint");
        nvs_clear_namespace("mqtt");

        ESP_LOGI(TAG, "Factory reset complete. Restarting device...");
        vTaskDelay(2500 / portTICK_PERIOD_MS);
        esp_restart();
    }
    else if (strcmp(action, "reboot") == 0)
    {
        task_reboot_system(NULL);
    }

    vTaskDelete(NULL);
}

//=================================================================
esp_err_t control_module_target(cJSON *json)
{
    cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
    if (!cJSON_IsString(action) || action->valuestring == NULL)
    {
        send_response_json("response", "control", "error", "missing or invalid 'action'");
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(action->valuestring, "reset") == 0 ||
        strcmp(action->valuestring, "reboot") == 0 ||
        strcmp(action->valuestring, "logout") == 0)
    {
        // Создаём копию строки для передачи в задачу
        char *action_copy = strdup(action->valuestring);
        if (action_copy == NULL)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for action string");
            return ESP_ERR_NO_MEM;
        }

        BaseType_t ret = xTaskCreate(action_task, "action_task", 4096,
                                     (void *)action_copy, 5, NULL);
        if (ret != pdTRUE)
        {
            free(action_copy);
            ESP_LOGE(TAG, "Failed to create action task");
            return ESP_FAIL;
        }
    }
    else
    {
        send_response_json("response", "control", "error", "unknown action");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}
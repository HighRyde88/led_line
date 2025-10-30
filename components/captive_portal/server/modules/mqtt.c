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

#include "mqtt.h"

static const char *TAG = "mqtt module";

static const config_param_t mqtt_params[] = {
    {"enable", "enable"},
    {"server", "server"},
    {"port", "port"},
    {"user", "user"},
    {"password", "password"},
};
//=================================================================
static void task_mqtt_stop(void *pvParameters)
{
    mqtt_client_handle_t client = (mqtt_client_handle_t)pvParameters;
    mqtt_client_stop(client);
    vTaskDelete(NULL);
}
//=================================================================
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    // esp_mqtt_event_handle_t event = event_data;
    // mqtt_client_handle_t client = (mqtt_client_handle_t)handler_args;

    ESP_LOGI(TAG, "MQTT test connection event: %d", (esp_mqtt_event_id_t)event_id);

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        send_response_json("event", "mqtt", "mqtt_connection_success", NULL);
        xTaskCreate(task_mqtt_stop, "mqtt_task_stop", 2048,
                    (void *)handler_args, 5, NULL);
        break;

    case MQTT_EVENT_ERROR:
        send_response_json("event", "mqtt", "mqtt_connection_failed", NULL);
        xTaskCreate(task_mqtt_stop, "mqtt_task_stop", 2048,
                    (void *)handler_args, 5, NULL);
        break;

    default:
        break;
    };
}
//=================================================================
esp_err_t mqtt_module_target(cJSON *json)
{
    cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
    if (!cJSON_IsString(action) || action->valuestring == NULL)
    {
        ESP_LOGW(TAG, "Invalid or missing 'action'");
        send_response_json("response", "mqtt", "error_mqtt", "\"invalid or missing 'action'\"");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ESP_OK;

    if (strcmp(action->valuestring, "save_partial") == 0)
    {
        cJSON *data_obj = cJSON_GetObjectItemCaseSensitive(json, "data");
        if (data_obj == NULL)
        {
            ESP_LOGW(TAG, "Missing 'data' in save request");
            send_response_json("response", "mqtt", "error_partial", "\"missing 'data'\"");
            return ESP_ERR_INVALID_ARG;
        }

        cJSON *port_item = cJSON_GetObjectItemCaseSensitive(data_obj, "port");
        if (port_item != NULL && cJSON_IsString(port_item))
        {
            int temp_port = atoi(port_item->valuestring);
            if (temp_port <= 0 || temp_port > 65535)
            {
                ESP_LOGE(TAG, "Invalid MQTT port provided: %s", port_item->valuestring);
                send_response_json("response", "mqtt", "error_partial", "invalid port provided (must be 1-65535)");
                return ESP_ERR_INVALID_ARG;
            }
        }

        // Проверка длины всех строковых полей
        cJSON *current_item = data_obj->child;
        while (current_item != NULL)
        {
            if (cJSON_IsString(current_item) && current_item->valuestring != NULL)
            {
                size_t str_len = strlen(current_item->valuestring);
                if (str_len > 31) // 32 байта, включая null-терминатор
                {
                    ESP_LOGE(TAG, "String field '%s' too long: %zu characters (max 31)", current_item->string, str_len);
                    send_response_json("response", "mqtt", "error_partial", "string field too long (max 31 characters)");
                    return ESP_ERR_INVALID_ARG;
                }
            }
            current_item = current_item->next;
        }

        result = parse_and_save_json_settings(
            "mqtt",
            data_obj,
            mqtt_params,
            sizeof(mqtt_params) / sizeof(mqtt_params[0]));

        if (result == ESP_OK)
        {
            send_response_json("response", "mqtt", "saved_partial", NULL);
        }
        else
        {
            ESP_LOGE(TAG, "Save failed: %s", esp_err_to_name(result));
            send_response_json("response", "mqtt", "error_partial", "save failed");
        }
    }
    else if (strcmp(action->valuestring, "load_partial") == 0)
    {
        cJSON *load_settings = load_and_parse_json_settings(
            "mqtt",
            mqtt_params,
            sizeof(mqtt_params) / sizeof(mqtt_params[0]));

        if (load_settings != NULL)
        {
            cJSON *response = cJSON_CreateObject();
            if (!response)
            {
                ESP_LOGE(TAG, "Failed to create JSON response");
                return ESP_ERR_NO_MEM;
            }

            cJSON_AddStringToObject(response, "type", "response");
            cJSON_AddStringToObject(response, "target", "mqtt");
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
            send_response_json("response", "mqtt", "error_partial", "load failed");
            return ESP_ERR_NOT_FOUND;
        }
    }
    else if (strcmp(action->valuestring, "test_connection") == 0)
    {
        cJSON *data_obj = cJSON_GetObjectItemCaseSensitive(json, "data");
        if (data_obj == NULL)
        {
            ESP_LOGW(TAG, "Missing 'data' in test request");
            send_response_json("response", "mqtt", "mqtt_test_error", "\"missing 'data'\"");
            return ESP_ERR_INVALID_ARG;
        }

        cJSON *server_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "server");
        cJSON *port_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "port");
        cJSON *user_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "user");
        cJSON *password_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "password");

        if ((server_obj != NULL && cJSON_IsString(server_obj) && server_obj->valuestring != NULL) &&
            (port_obj != NULL && cJSON_IsString(port_obj) && port_obj->valuestring != NULL) &&
            (user_obj != NULL && cJSON_IsString(user_obj) && user_obj->valuestring != NULL) &&
            (password_obj != NULL && cJSON_IsString(password_obj) && password_obj->valuestring != NULL))
        {
            char mqtt_uri[64] = {0};
            sniprintf(mqtt_uri, sizeof(mqtt_uri), "mqtt://%s:%s", server_obj->valuestring, port_obj->valuestring);

            mqtt_config_t mqtt_config = {
                .server_uri = mqtt_uri,
                .client_id = "esp32TestConnectionClient",
                .username = user_obj->valuestring,
                .password = password_obj->valuestring,
                .auto_reconnect = false};

            mqtt_client_handle_t test_client = mqtt_client_start(&mqtt_config, mqtt_event_handler);

            if (test_client != NULL)
            {
                ESP_LOGI(TAG, "MQTT test connection success");
                send_response_json("response", "mqtt", "mqtt_test_ok", "success test started");
            }
            else
            {
                ESP_LOGE(TAG, "MQTT test connection failed");
                send_response_json("response", "mqtt", "mqtt_test_error", "failed test started");
            }
        }
        else
        {
            send_response_json("response", "mqtt", "mqtt_test_error", "value error");
            return ESP_ERR_INVALID_ARG;
        }
    }
    else
    {
        ESP_LOGW(TAG, "Unknown action: %s", action->valuestring);
        send_response_json("response", "mqtt", "error_mqtt", "unknown action");
        return ESP_ERR_INVALID_ARG;
    }

    return result;
}
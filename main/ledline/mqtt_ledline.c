#include "ledline.h"
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "mqtt_ledline.h"
#include "effects_ledline.h"

int topic_count = 0;
char **topic_list = NULL;

static char hostname_str[32] = {0};
static bool isSubscribed = false;
static bool initialized = false;

static const char *default_topics[] = {
    "ledline/state",
    "ledline/color",
    "ledline/brightness",
    "ledline/mode",
    "ledline/pause"};
static const int default_topic_count = 5;

static const char *TAG = "led_strip_mqtt";

mqtt_client_handle_t mqttClient = NULL;

//=================================================================
static void ledline_set_mqtt_topics(void)
{
    if (topic_list != NULL)
    {
        for (int i = 0; i < topic_count; i++)
        {
            free(topic_list[i]);
        }
        free(topic_list);
        topic_list = NULL;
    }

    if (strlen(hostname_str) == 0)
    {
        ESP_LOGE(TAG, "Hostname is empty, cannot create topics");
        topic_count = 0;
        return;
    }

    if (default_topic_count <= 0)
    {
        topic_count = 0;
        return;
    }

    topic_list = malloc(default_topic_count * sizeof(char *));
    if (topic_list == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for topic list");
        topic_count = 0;
        return;
    }

    memset(topic_list, 0, default_topic_count * sizeof(char *));

    topic_count = default_topic_count;
    for (int i = 0; i < default_topic_count; i++)
    {
        if (default_topics[i] == NULL)
        {
            free(topic_list[i]);
            topic_list[i] = NULL;
            continue;
        }

        int full_topic_len = strlen(hostname_str) + strlen(default_topics[i]) + 2; // +1 для '/' +1 для '\0'
        topic_list[i] = malloc(full_topic_len);
        if (topic_list[i] != NULL)
        {
            int ret = snprintf(topic_list[i], full_topic_len, "%s/%s", hostname_str, default_topics[i]);
            if (ret < 0 || ret >= full_topic_len)
            {
                ESP_LOGE(TAG, "Topic string construction failed for topic %d", i);
                free(topic_list[i]);
                topic_list[i] = NULL;
                continue;
            }
            ESP_LOGI(TAG, "Topic %d: %s", i, topic_list[i]);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to allocate memory for topic %d", i);
        }
    }
}

//=================================================================
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    mqtt_client_handle_t client = (mqtt_client_handle_t)handler_args;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected");
        if (!isSubscribed && topic_list != NULL && topic_count > 0)
        {
            for (int i = 0; i < topic_count; i++)
            {
                if (topic_list[i] != NULL)
                {
                    esp_err_t err = mqtt_client_subscribe(client, topic_list[i], 0);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Failed to subscribe to topic %s, error: %s",
                                 topic_list[i], esp_err_to_name(err));
                    }
                    else
                    {
                        ESP_LOGI(TAG, "Subscribed to topic: %s", topic_list[i]);
                    }
                }
            }
            isSubscribed = true;

            /*
            mqtt_publish_state("state", current_state ? "enable" : "disable");

            char brightness_str[4] = {0};
            sniprintf(brightness_str, sizeof(brightness_str), "%d", (int)((stored_brightness / 255.0) * 100));
            mqtt_publish_state("brightness", brightness_str);

            uint32_t color_int = color_from_hsv(current_color);
            char color_str[16] = {0};
            snprintf(color_str, sizeof(color_str), "#%06lX", color_int & 0x00FFFFFF);
            mqtt_publish_state("color", color_str);

            mqtt_publish_state("mode", "static");
            */
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT Disconnected");
        isSubscribed = false;
        break;

    case MQTT_EVENT_DATA:
        if (event->topic != NULL && event->data != NULL)
        {
            for (int i = 0; i < topic_count; i++)
            {
                if (topic_list[i] != NULL &&
                    event->topic_len == strlen(topic_list[i]) &&
                    strncmp(event->topic, topic_list[i], event->topic_len) == 0)
                {
                    mqtt_data_t data_message = {0};
                    bool cleanup_needed = false;

                    data_message.data = strndup(event->data, event->data_len);
                    if (data_message.data == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to allocate memory for data");
                        cleanup_needed = true;
                        goto cleanup;
                    }

                    data_message.topic = strndup(event->topic, event->topic_len);
                    if (data_message.topic == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to allocate memory for topic");
                        cleanup_needed = true;
                        goto cleanup;
                    }

                    BaseType_t result = xQueueSend(mqttQueue, &data_message, 50 / portTICK_PERIOD_MS);
                    if (result != pdTRUE)
                    {
                        ESP_LOGW(TAG, "Failed to send message to queue, queue full");
                        cleanup_needed = true;
                        goto cleanup;
                    }
                    break;

                cleanup:
                    if (cleanup_needed)
                    {
                        free(data_message.data);
                        free(data_message.topic);
                    }
                    break;
                }
            }
        }
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT Subscribed, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT Unsubscribed, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT Published, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT Error, error_type=%d", event->error_handle->error_type);
        break;

    default:
        ESP_LOGD(TAG, "MQTT Event, id=%d", event_id);
        break;
    }
}

//=================================================================
esp_err_t mqtt_ledline_resources_init(void)
{
    if (initialized)
    {
        ESP_LOGW(TAG, "MQTT already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    char enable_str[16] = {0};
    size_t str_size = sizeof(enable_str);
    esp_err_t result = nvs_load_data("mqtt", "enable", enable_str, &str_size, NVS_TYPE_STR);

    if (result == ESP_OK)
    {
        if (strcmp(enable_str, "true") == 0)
        {
            ESP_LOGI(TAG, "MQTT is enabled. Loading MQTT configuration...");

            char server_str[64] = {0};
            char port_str[8] = {0};
            char user_str[32] = {0};
            char password_str[64] = {0};
            char temp_hostname_str[32] = {0};

            str_size = sizeof(server_str);
            esp_err_t server_result = nvs_load_data("mqtt", "server", server_str, &str_size, NVS_TYPE_STR);
            if (server_result != ESP_OK || strlen(server_str) == 0)
            {
                ESP_LOGE(TAG, "Failed to load MQTT server from NVS: %s", esp_err_to_name(server_result));
                return server_result != ESP_OK ? server_result : ESP_ERR_INVALID_ARG;
            }
            ESP_LOGI(TAG, "Loaded MQTT server: %s", server_str);

            str_size = sizeof(port_str);
            esp_err_t port_result = nvs_load_data("mqtt", "port", port_str, &str_size, NVS_TYPE_STR);
            if (port_result != ESP_OK || strlen(port_str) == 0)
            {
                ESP_LOGE(TAG, "Failed to load MQTT port from NVS: %s", esp_err_to_name(port_result));
                return port_result != ESP_OK ? port_result : ESP_ERR_INVALID_ARG;
            }

            int port = atoi(port_str);
            if (port <= 0 || port > 65535)
            {
                ESP_LOGE(TAG, "Invalid MQTT port: %s", port_str);
                return ESP_ERR_INVALID_ARG;
            }
            ESP_LOGI(TAG, "Loaded MQTT port: %s", port_str);

            str_size = sizeof(user_str);
            esp_err_t user_result = nvs_load_data("mqtt", "user", user_str, &str_size, NVS_TYPE_STR);
            if (user_result != ESP_OK || strlen(user_str) == 0)
            {
                ESP_LOGE(TAG, "Failed to load MQTT user from NVS: %s", esp_err_to_name(user_result));
                return user_result != ESP_OK ? user_result : ESP_ERR_INVALID_ARG;
            }

            str_size = sizeof(password_str);
            esp_err_t pass_result = nvs_load_data("mqtt", "password", password_str, &str_size, NVS_TYPE_STR);
            if (pass_result != ESP_OK || strlen(password_str) == 0)
            {
                ESP_LOGE(TAG, "Failed to load MQTT password from NVS: %s", esp_err_to_name(pass_result));
                return pass_result != ESP_OK ? pass_result : ESP_ERR_INVALID_ARG;
            }

            str_size = sizeof(temp_hostname_str);
            esp_err_t hostname_result = nvs_load_data("device", "hostname", temp_hostname_str, &str_size, NVS_TYPE_STR);
            if (hostname_result != ESP_OK || strlen(temp_hostname_str) == 0)
            {
                ESP_LOGW(TAG, "Failed to load hostname from NVS: %s, using default",
                         esp_err_to_name(hostname_result));
                strncpy(temp_hostname_str, "esp32device", sizeof(temp_hostname_str) - 1);
            }
            else
            {
                ESP_LOGI(TAG, "Loaded hostname: %s", temp_hostname_str);
            }

            if (strlen(temp_hostname_str) == 0)
            {
                ESP_LOGE(TAG, "Hostname is empty, cannot proceed");
                return ESP_ERR_INVALID_ARG;
            }

            // Усечение hostname с предупреждением
            if (strlen(temp_hostname_str) >= sizeof(hostname_str))
            {
                ESP_LOGW(TAG, "Hostname truncated from %d to %d chars", (int)strlen(temp_hostname_str), (int)(sizeof(hostname_str) - 1));
            }
            strncpy(hostname_str, temp_hostname_str, sizeof(hostname_str) - 1);
            hostname_str[sizeof(hostname_str) - 1] = '\0';

            char mqtt_uri[128] = {0};
            int ret = snprintf(mqtt_uri, sizeof(mqtt_uri), "mqtt://%s:%s", server_str, port_str);
            if (ret < 0 || ret >= sizeof(mqtt_uri))
            {
                ESP_LOGE(TAG, "MQTT URI buffer overflow");
                return ESP_ERR_INVALID_ARG;
            }

            mqtt_config_t mqtt_config = {
                .server_uri = mqtt_uri,
                .client_id = hostname_str,
                .username = user_str,
                .password = password_str,
                .auto_reconnect = true,
            };

            ledline_set_mqtt_topics();

            if (topic_count == 0 || topic_list == NULL)
            {
                ESP_LOGE(TAG, "Failed to create MQTT topics");
                vQueueDelete(mqttQueue);
                mqttQueue = NULL;
                return ESP_FAIL;
            }

            mqttClient = mqtt_client_start(&mqtt_config, mqtt_event_handler);
            if (mqttClient == NULL)
            {
                ESP_LOGE(TAG, "Failed to start MQTT client");
                vQueueDelete(mqttQueue);
                mqttQueue = NULL;
                return ESP_FAIL;
            }

            initialized = true; // <-- Установка флага инициализации
            ESP_LOGI(TAG, "MQTT configuration loaded successfully");
            return ESP_OK;
        }
        else
        {
            ESP_LOGI(TAG, "MQTT is disabled");
            return ESP_OK;
        }
    }
    else
    {
        ESP_LOGI(TAG, "MQTT configuration not found in NVS");
        return ESP_OK;
    }

    return ESP_OK;
}

//=================================================================
void mqtt_ledline_resources_deinit(void)
{
    if (!initialized)
    {
        return;
    }

    if (mqttClient != NULL)
    {
        mqtt_client_stop(mqttClient);
        mqttClient = NULL;
    }

    if (mqttQueue != NULL)
    {
        mqtt_data_t item;
        while (xQueueReceive(mqttQueue, &item, 0) == pdTRUE)
        {
            free(item.data);
            free(item.topic);
        }

        vQueueDelete(mqttQueue);
        mqttQueue = NULL;
    }

    if (topic_list != NULL)
    {
        for (int i = 0; i < topic_count; i++)
        {
            free(topic_list[i]);
        }
        free(topic_list);
        topic_list = NULL;
        topic_count = 0;
    }

    isSubscribed = false;
    memset(hostname_str, 0, sizeof(hostname_str));
    initialized = false; // <-- Сброс флага
}

//=================================================================
esp_err_t mqtt_publish_state(const char *topic_suffix, const char *payload)
{
    if (!isSubscribed || mqttClient == NULL || topic_suffix == NULL || payload == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // Проверяем, что topic_suffix соответствует одному из default_topics
    bool is_valid_topic = false;
    for (int i = 0; i < default_topic_count; i++)
    {
        if (strcmp(default_topics[i] + strlen("ledline/"), topic_suffix) == 0)
        {
            is_valid_topic = true;
            break;
        }
    }

    if (!is_valid_topic)
    {
        ESP_LOGW(TAG, "Invalid topic suffix: %s", topic_suffix);
        return ESP_ERR_INVALID_ARG;
    }

    // Формируем полный топик: hostname/ledline/<topic_suffix>/status
    int full_topic_len = strlen(hostname_str) + strlen("ledline/") + strlen(topic_suffix) + strlen("/status") + 2; // '/' + '\0'
    char *full_topic = malloc(full_topic_len);
    if (full_topic == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for full topic");
        return ESP_ERR_NO_MEM;
    }

    int ret = snprintf(full_topic, full_topic_len, "%s/ledline/%s/status", hostname_str, topic_suffix);
    if (ret < 0 || ret >= full_topic_len)
    {
        ESP_LOGE(TAG, "Topic string construction failed");
        free(full_topic);
        return ESP_ERR_INVALID_ARG;
    }

    // Публикуем сообщение
    int msg_id = mqtt_client_publish(mqttClient, full_topic, payload, strlen(payload), 0, 0);

    // Логируем до освобождения памяти
    if (msg_id < 0)
    {
        ESP_LOGE(TAG, "Failed to publish to topic: %s", full_topic);
        free(full_topic);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Published to topic: %s, payload: %s", full_topic, payload);
    free(full_topic);
    return ESP_OK;
}
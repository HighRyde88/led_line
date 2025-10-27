#include "ledline.h"
#include <stdlib.h>
#include <string.h>

static char hostname_str[32] = {0};
static bool isSubscribed = false;
static char **topic_list = NULL;
static int topic_count = 0;

static const char *default_topics[] = {
    "ledline/state"};
static const int default_topic_count = 3;

static const char *TAG = "led_strip_mqtt";
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
            continue;

        int full_topic_len = strlen(hostname_str) + strlen(default_topics[i]) + 2;
        topic_list[i] = malloc(full_topic_len);
        if (topic_list[i] != NULL)
        {
            snprintf(topic_list[i], full_topic_len, "%s/%s", hostname_str, default_topics[i]);
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

    ESP_LOGI(TAG, "MQTT test connection event: %d", (esp_mqtt_event_id_t)event_id);

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        if (!isSubscribed && topic_list != NULL && topic_count > 0)
        {
            for (int i = 0; i < topic_count; i++)
            {
                if (topic_list[i] != NULL)
                {
                    mqtt_client_subscribe(client, topic_list[i], 0);
                    ESP_LOGI(TAG, "Subscribed to topic: %s", topic_list[i]);
                }
            }
            isSubscribed = true;
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        isSubscribed = false;
        ESP_LOGI(TAG, "MQTT disconnected");
        break;

    default:
        break;
    };
}
//=================================================================
esp_err_t mqtt_ledline_resources_init(void)
{
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
            char password_str[32] = {0};
            char temp_hostname_str[32] = {0}; // Временный буфер для чтения

            str_size = sizeof(server_str);
            esp_err_t server_result = nvs_load_data("mqtt", "server", server_str, &str_size, NVS_TYPE_STR);
            if (server_result != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to load MQTT server from NVS: %s", esp_err_to_name(server_result));
                return server_result;
            }
            ESP_LOGI(TAG, "Loaded MQTT server: %s", server_str);

            str_size = sizeof(port_str);
            esp_err_t port_result = nvs_load_data("mqtt", "port", port_str, &str_size, NVS_TYPE_STR);
            if (port_result != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to load MQTT port from NVS: %s", esp_err_to_name(port_result));
                return port_result;
            }
            ESP_LOGI(TAG, "Loaded MQTT port: %s", port_str);

            str_size = sizeof(user_str);
            esp_err_t user_result = nvs_load_data("mqtt", "user", user_str, &str_size, NVS_TYPE_STR);
            if (user_result != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to load MQTT user from NVS: %s", esp_err_to_name(user_result));
                return user_result;
            }
            ESP_LOGI(TAG, "Loaded MQTT user: %s", user_str);

            str_size = sizeof(password_str);
            esp_err_t pass_result = nvs_load_data("mqtt", "password", password_str, &str_size, NVS_TYPE_STR);
            if (pass_result != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to load MQTT password from NVS: %s", esp_err_to_name(pass_result));
                return pass_result;
            }
            ESP_LOGI(TAG, "Loaded MQTT password: %s", password_str);

            str_size = sizeof(temp_hostname_str);
            esp_err_t hostname_result = nvs_load_data("device", "hostname", temp_hostname_str, &str_size, NVS_TYPE_STR);
            if (hostname_result != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to load hostname from NVS: %s, using default", esp_err_to_name(hostname_result));
                strcpy(temp_hostname_str, "esp32device");
            }
            else
            {
                ESP_LOGI(TAG, "Loaded hostname: %s", temp_hostname_str);
            }

            strcpy(hostname_str, temp_hostname_str);

            char mqtt_uri[128] = {0};
            snprintf(mqtt_uri, sizeof(mqtt_uri), "mqtt://%s:%s", server_str, port_str);

            mqtt_config_t mqtt_config = {
                .server_uri = mqtt_uri,
                .client_id = hostname_str,
                .username = user_str,
                .password = password_str,
                .auto_reconnect = true,
                .reconnect_timeout_sec = 5};

            ledline_set_mqtt_topics();

            mqtt_client_start(&mqtt_config, mqtt_event_handler);

            ESP_LOGI(TAG, "MQTT configuration loaded successfully");
        }
        else
        {
            ESP_LOGI(TAG, "MQTT is disabled");
        }
    }
    else
    {
        ESP_LOGI(TAG, "MQTT configuration not found in NVS");
    }

    return ESP_OK;
}
//=================================================================
void mqtt_ledline_resources_deinit(void)
{
    if (topic_list != NULL)
    {
        for (int i = 0; i < topic_count; i++)
        {
            free(topic_list[i]);
        }
        free(topic_list);
        topic_list = NULL;
        topic_count = 0;
        isSubscribed = false;
    }
}
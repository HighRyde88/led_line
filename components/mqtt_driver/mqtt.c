#include "mqtt.h"
#include "esp_log.h"
#include "esp_event.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_random.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "DMQTT";

typedef enum {
    MQTT_CLIENT_STATE_STOPPED = 0,
    MQTT_CLIENT_STATE_STARTED,
    MQTT_CLIENT_STATE_STOPPING
} mqtt_client_state_t;

typedef struct {
    esp_mqtt_client_handle_t mqtt_client;
    SemaphoreHandle_t mqtt_mutex;
    esp_event_handler_t user_mqtt_event_handler;
    mqtt_client_state_t state;
} mqtt_client_data_t;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    mqtt_client_data_t *client_data = (mqtt_client_data_t *)handler_args;

    // Проверить состояние клиента под защитой mutex
    if (!xSemaphoreTake(client_data->mqtt_mutex, pdMS_TO_TICKS(100))) {
        return; // Не удалось получить доступ
    }

    if (client_data->state != MQTT_CLIENT_STATE_STARTED) {
        xSemaphoreGive(client_data->mqtt_mutex);
        return;
    }

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        //printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        //printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }

    // Копируем указатель на пользовательский обработчик безопасно
    esp_event_handler_t user_handler = client_data->user_mqtt_event_handler;
    xSemaphoreGive(client_data->mqtt_mutex);

    if (user_handler) {
        user_handler(handler_args, base, event_id, event_data);
    }
}

//=================================================================
mqtt_client_handle_t mqtt_client_start(const mqtt_config_t *config, esp_event_handler_t event_handler)
{
    if (!config || !config->server_uri)
    {
        ESP_LOGE(TAG, "Config or server_uri is NULL");
        return NULL;
    }

    mqtt_client_data_t *client_data = calloc(1, sizeof(mqtt_client_data_t));
    if (!client_data)
    {
        ESP_LOGE(TAG, "Failed to allocate client data");
        return NULL;
    }

    client_data->mqtt_mutex = xSemaphoreCreateMutex();
    if (!client_data->mqtt_mutex)
    {
        ESP_LOGE(TAG, "Failed to create MQTT mutex");
        free(client_data);
        return NULL;
    }

    client_data->user_mqtt_event_handler = event_handler;
    client_data->state = MQTT_CLIENT_STATE_STOPPED;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = config->server_uri,
        .credentials.username = config->username,
        .credentials.authentication.password = config->password,
        .credentials.client_id = config->client_id,
        .session.last_will.topic = config->lwt_topic,
        .session.last_will.msg = config->lwt_msg,
        .session.last_will.qos = config->lwt_qos,
        .session.last_will.retain = config->lwt_retain,
        .session.disable_clean_session = config->disable_clean_session,
        .session.keepalive = config->keepalive > 0 ? config->keepalive : 60,
        .network.disable_auto_reconnect = !config->auto_reconnect,
        .network.reconnect_timeout_ms = (config->reconnect_timeout_sec > 0 ? config->reconnect_timeout_sec : 5) * 1000,
    };

    esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        vSemaphoreDelete(client_data->mqtt_mutex);
        free(client_data);
        return NULL;
    }

    esp_err_t err = esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, client_data);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register MQTT event handler: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(mqtt_client);
        vSemaphoreDelete(client_data->mqtt_mutex);
        free(client_data);
        return NULL;
    }

    err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(mqtt_client);
        vSemaphoreDelete(client_data->mqtt_mutex);
        free(client_data);
        return NULL;
    }

    // Устанавливаем состояние после успешного старта
    if (!xSemaphoreTake(client_data->mqtt_mutex, pdMS_TO_TICKS(1000)))
    {
        ESP_LOGE(TAG, "Failed to acquire mutex during start");
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
        vSemaphoreDelete(client_data->mqtt_mutex);
        free(client_data);
        return NULL;
    }

    client_data->mqtt_client = mqtt_client;
    client_data->state = MQTT_CLIENT_STATE_STARTED;
    xSemaphoreGive(client_data->mqtt_mutex);

    ESP_LOGI(TAG, "MQTT client started");
    return (mqtt_client_handle_t)client_data;
}
//=================================================================
esp_err_t mqtt_client_stop(mqtt_client_handle_t client)
{
    mqtt_client_data_t *client_data = (mqtt_client_data_t *)client;
    if (!client_data) {
        ESP_LOGE(TAG, "Client is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!xSemaphoreTake(client_data->mqtt_mutex, pdMS_TO_TICKS(1000)))
    {
        ESP_LOGE(TAG, "Failed to acquire mutex during stop");
        return ESP_ERR_TIMEOUT;
    }

    if (client_data->state != MQTT_CLIENT_STATE_STARTED) {
        xSemaphoreGive(client_data->mqtt_mutex);
        ESP_LOGE(TAG, "Client is not in started state");
        return ESP_ERR_INVALID_STATE;
    }

    // Меняем состояние на STOPPING
    client_data->state = MQTT_CLIENT_STATE_STOPPING;
    esp_mqtt_client_handle_t mqtt_client = client_data->mqtt_client;
    client_data->mqtt_client = NULL;

    xSemaphoreGive(client_data->mqtt_mutex);

    // Останавливаем и уничтожаем клиент вне критической секции
    if (mqtt_client) {
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
    }

    // Освобождаем ресурсы
    if (!xSemaphoreTake(client_data->mqtt_mutex, pdMS_TO_TICKS(1000)))
    {
        ESP_LOGE(TAG, "Failed to acquire mutex during cleanup");
        return ESP_ERR_TIMEOUT;
    }

    client_data->state = MQTT_CLIENT_STATE_STOPPED;
    xSemaphoreGive(client_data->mqtt_mutex);

    vSemaphoreDelete(client_data->mqtt_mutex);
    free(client_data);

    ESP_LOGI(TAG, "MQTT client stopped and destroyed");
    return ESP_OK;
}
//=================================================================
esp_err_t mqtt_client_publish(mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain)
{
    mqtt_client_data_t *client_data = (mqtt_client_data_t *)client;
    if (!client_data || !topic)
    {
        ESP_LOGE(TAG, "Client not initialized or topic is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!xSemaphoreTake(client_data->mqtt_mutex, pdMS_TO_TICKS(1000)))
    {
        ESP_LOGE(TAG, "Failed to acquire mutex for publish");
        return ESP_ERR_TIMEOUT;
    }

    if (client_data->state != MQTT_CLIENT_STATE_STARTED) {
        xSemaphoreGive(client_data->mqtt_mutex);
        ESP_LOGE(TAG, "Client is not in started state");
        return ESP_ERR_INVALID_STATE;
    }

    esp_mqtt_client_handle_t mqtt_client = client_data->mqtt_client;
    xSemaphoreGive(client_data->mqtt_mutex);

    if (!mqtt_client) {
        ESP_LOGE(TAG, "MQTT client is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, data, len, qos, retain);

    if (msg_id == -1)
    {
        ESP_LOGE(TAG, "Failed to publish message to topic: %s", topic);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Published message (ID: %d) to topic: %s", msg_id, topic);
    return ESP_OK;
}
//=================================================================
esp_err_t mqtt_client_subscribe(mqtt_client_handle_t client, const char *topic, int qos)
{
    mqtt_client_data_t *client_data = (mqtt_client_data_t *)client;
    if (!client_data || !topic)
    {
        ESP_LOGE(TAG, "Client not initialized or topic is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!xSemaphoreTake(client_data->mqtt_mutex, pdMS_TO_TICKS(1000)))
    {
        ESP_LOGE(TAG, "Failed to acquire mutex for subscribe");
        return ESP_ERR_TIMEOUT;
    }

    if (client_data->state != MQTT_CLIENT_STATE_STARTED) {
        xSemaphoreGive(client_data->mqtt_mutex);
        ESP_LOGE(TAG, "Client is not in started state");
        return ESP_ERR_INVALID_STATE;
    }

    esp_mqtt_client_handle_t mqtt_client = client_data->mqtt_client;
    xSemaphoreGive(client_data->mqtt_mutex);

    if (!mqtt_client) {
        ESP_LOGE(TAG, "MQTT client is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    int msg_id = esp_mqtt_client_subscribe(mqtt_client, topic, qos);

    if (msg_id == -1)
    {
        ESP_LOGE(TAG, "Failed to subscribe to topic: %s", topic);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Subscribed to topic: %s (ID: %d)", topic, msg_id);
    return ESP_OK;
}
//=================================================================
esp_err_t mqtt_client_unsubscribe(mqtt_client_handle_t client, const char *topic)
{
    mqtt_client_data_t *client_data = (mqtt_client_data_t *)client;
    if (!client_data || !topic)
    {
        ESP_LOGE(TAG, "Client not initialized or topic is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!xSemaphoreTake(client_data->mqtt_mutex, pdMS_TO_TICKS(1000)))
    {
        ESP_LOGE(TAG, "Failed to acquire mutex for unsubscribe");
        return ESP_ERR_TIMEOUT;
    }

    if (client_data->state != MQTT_CLIENT_STATE_STARTED) {
        xSemaphoreGive(client_data->mqtt_mutex);
        ESP_LOGE(TAG, "Client is not in started state");
        return ESP_ERR_INVALID_STATE;
    }

    esp_mqtt_client_handle_t mqtt_client = client_data->mqtt_client;
    xSemaphoreGive(client_data->mqtt_mutex);

    if (!mqtt_client) {
        ESP_LOGE(TAG, "MQTT client is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    int msg_id = esp_mqtt_client_unsubscribe(mqtt_client, topic);

    if (msg_id == -1)
    {
        ESP_LOGE(TAG, "Failed to unsubscribe from topic: %s", topic);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Unsubscribed from topic: %s (ID: %d)", topic, msg_id);
    return ESP_OK;
}
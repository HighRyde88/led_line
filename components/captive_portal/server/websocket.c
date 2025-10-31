#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "server/server.h"
#include "modules/modules.h"

#define WS_SEND_QUEUE_SIZE 10
#define WS_TASK_STACK_SIZE 4096
#define WS_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

static QueueHandle_t ws_send_queue = NULL;
static httpd_handle_t ws_server = NULL;
static int client_socket = -1;
static TaskHandle_t sender_task_handle = NULL;
static SemaphoreHandle_t socket_mutex = NULL;
static volatile bool server_stopped = false;

static const char *TAG = "WS";

// Forward declarations
static bool ws_server_send_text(const char *data, size_t len);
static void websocket_router(cJSON *json);
static void on_http_client_disconnect(httpd_handle_t server, int sockfd);
static esp_err_t websocket_handler(httpd_req_t *req);
static void ws_sender_task(void *pvParameters);

// Message structure
typedef struct
{
    char *payload;
    size_t len;
} ws_msg_t;

// Target handler type
typedef esp_err_t (*ws_target_handler_t)(cJSON *json);

// Target routing table
typedef struct
{
    const char *target;
    ws_target_handler_t handler;
} ws_target_route_t;

// Forward declaration for websocket target
static esp_err_t websocket_module_target(cJSON *json);

static const ws_target_route_t target_routes[] = {
    {"control", control_module_target},
    {"device", device_module_target},
    {"ledstrip", ledstrip_module_target},
    {"wifi", wifi_module_target},
    {"network", network_module_target},
    {"apoint", apoint_module_target},
    {"mqtt", mqtt_module_target},
    {"update", update_module_target},
    {"websocket", websocket_module_target}  // Добавлено
};

static const size_t target_routes_count = sizeof(target_routes) / sizeof(target_routes[0]);

//=================================================================
// Websocket target handler
//=================================================================
static esp_err_t websocket_module_target(cJSON *json)
{
    cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
    if (!cJSON_IsString(action) || !action->valuestring)
    {
        send_response_json("response", "websocket", "error_action", "missing or invalid 'action'", false);
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(action->valuestring, "ping") == 0)
    {
        // Отправляем pong
        send_response_json("response", "websocket", "pong", NULL, false);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Unknown action for websocket target: %s", action->valuestring);
    send_response_json("response", "websocket", "error_action", "unknown action", false);
    return ESP_ERR_INVALID_ARG;
}

//=================================================================
// Client disconnect handler
//=================================================================
static void on_http_client_disconnect(httpd_handle_t server, int sockfd)
{
    ESP_LOGI(TAG, "Client disconnected (sockfd=%d)", sockfd);

    if (socket_mutex && !server_stopped)
    {
        if (xSemaphoreTake(socket_mutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            if (client_socket == sockfd)
            {
                client_socket = -1;
                ESP_LOGI(TAG, "Cleared client_socket for sockfd=%d", sockfd);
            }
            xSemaphoreGive(socket_mutex);
        }
    }
}

//=================================================================
// Command router
//=================================================================
static void websocket_router(cJSON *json)
{
    cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (!cJSON_IsString(type) || !type->valuestring)
    {
        send_response_json("response", "invalid", "error_type", "missing or invalid 'type'", false);
        return;
    }

    if (strcmp(type->valuestring, "request") != 0)
    {
        send_response_json("response", "invalid", "error_type", "unknown type", false);
        return;
    }

    cJSON *target = cJSON_GetObjectItemCaseSensitive(json, "target");
    if (!cJSON_IsString(target) || !target->valuestring)
    {
        send_response_json("response", "invalid", "error_target", "missing or invalid 'target'", false);
        return;
    }

    for (size_t i = 0; i < target_routes_count; i++)
    {
        if (strcmp(target->valuestring, target_routes[i].target) == 0)
        {
            target_routes[i].handler(json);
            return;
        }
    }

    ESP_LOGW(TAG, "Unknown target: %s", target->valuestring);
    send_response_json("response", target->valuestring, "error_target", "unknown target", false);
}

//=================================================================
// WebSocket handler
//=================================================================
static esp_err_t websocket_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
    {
        int new_sockfd = httpd_req_to_sockfd(req);
        ESP_LOGI(TAG, "New WebSocket connection (sockfd=%d)", new_sockfd);

        if (xSemaphoreTake(socket_mutex, portMAX_DELAY) == pdTRUE)
        {
            if (client_socket != -1 && client_socket != new_sockfd)
            {
                ESP_LOGW(TAG, "Closing previous WebSocket client (sockfd=%d)", client_socket);
                httpd_sess_trigger_close(ws_server, client_socket);
            }
            client_socket = new_sockfd;
            xSemaphoreGive(socket_mutex);

            send_response_json("event", "system", "ws_ready", NULL, false);
            ESP_LOGI(TAG, "Sent 'ready' event to client");
        }
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt = {0};
    uint8_t *buf = NULL;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get frame len: %s", esp_err_to_name(ret));
        return ret;
    }

    if (ws_pkt.len == 0)
        return ESP_OK;

    buf = calloc(1, ws_pkt.len + 1);
    if (!buf)
    {
        ESP_LOGE(TAG, "Memory allocation failed");
        return ESP_ERR_NO_MEM;
    }
    ws_pkt.payload = buf;

    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read frame: %s", esp_err_to_name(ret));
        free(buf);
        return ret;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT)
    {
        ESP_LOGI(TAG, "Received text: %.*s", ws_pkt.len, buf);
        cJSON *json = cJSON_Parse((char *)buf);
        if (json)
        {
            websocket_router(json); // Обработка JSON-команд
            cJSON_Delete(json);
        }
        else
        {
            ESP_LOGW(TAG, "JSON parse error: %s", cJSON_GetErrorPtr());
            send_response_json("response", "system", "error_json", "invalid json", false);
        }
    }
    else
    {
        ESP_LOGW(TAG, "Unknown frame type: %d", ws_pkt.type);
    }

    free(buf);
    return ESP_OK;
}

//=================================================================
// WebSocket sender task
//=================================================================
static void ws_sender_task(void *pvParameters)
{
    ws_msg_t msg;

    while (!server_stopped)
    {
        if (xQueueReceive(ws_send_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            if (msg.payload == NULL)
                break;

            int sock = -1;
            if (xSemaphoreTake(socket_mutex, pdMS_TO_TICKS(20)) == pdTRUE)
            {
                sock = client_socket;
                xSemaphoreGive(socket_mutex);
            }

            if (sock == -1 || !ws_server || server_stopped)
            {
                ESP_LOGW(TAG, "No client connected. Dropping message.");
                free(msg.payload); // ✅ Освобождаем память
                continue;
            }

            httpd_ws_frame_t ws_pkt = {
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)msg.payload,
                .len = msg.len};

            esp_err_t ret = httpd_ws_send_frame_async(ws_server, sock, &ws_pkt);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to send message: %s", esp_err_to_name(ret));

                // Попробуем закрыть сессию, если ошибка критическая
                if (ret == ESP_ERR_INVALID_ARG || ret == ESP_ERR_INVALID_STATE)
                {
                    ESP_LOGW(TAG, "Triggering close due to send error.");
                    httpd_sess_trigger_close(ws_server, sock);
                }
            }
            else
            {
                ESP_LOGI(TAG, "Transmited text: %.*s", msg.len, msg.payload);
            }

            free(msg.payload);
        }
    }

    // Очистка оставшихся сообщений в очереди при завершении
    ws_msg_t cleanup_msg;
    while (xQueueReceive(ws_send_queue, &cleanup_msg, 0) == pdTRUE)
    {
        if (cleanup_msg.payload)
        {
            free(cleanup_msg.payload);
        }
    }

    sender_task_handle = NULL;
    vTaskDelete(NULL);
}

//=================================================================
// Start WebSocket server
//=================================================================
void captive_portal_ws_server_start(esp_netif_t *netif)
{
    if (netif == NULL)
    {
        ESP_LOGE(TAG, "Invalid netif provided");
        return;
    }

    if (ws_server)
    {
        ESP_LOGW(TAG, "WebSocket server already running");
        return;
    }

    server_stopped = false;

    // Create resources
    if (socket_mutex == NULL)
    {
        socket_mutex = xSemaphoreCreateMutex();
    }

    if (ws_send_queue == NULL)
    {
        ws_send_queue = xQueueCreate(WS_SEND_QUEUE_SIZE, sizeof(ws_msg_t));
    }

    if (!socket_mutex || !ws_send_queue)
    {
        ESP_LOGE(TAG, "Failed to create resources");
        captive_portal_ws_server_stop(); // Cleanup
        return;
    }

    // Start sender task only if not already running
    if (sender_task_handle == NULL)
    {
        if (xTaskCreate(ws_sender_task, "ws_sender", WS_TASK_STACK_SIZE, NULL,
                        WS_TASK_PRIORITY, &sender_task_handle) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to create sender task");
            captive_portal_ws_server_stop(); // Cleanup
            return;
        }
    }

    // Configure and start HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8810;
    config.ctrl_port = ESP_HTTPD_DEF_CTRL_PORT - 1;
    config.close_fn = on_http_client_disconnect;
    config.max_open_sockets = 1; // Ограничить до 1 соединения
    config.stack_size = WS_TASK_STACK_SIZE;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 60;   // Таймаут ожидания данных
    config.send_wait_timeout = 60;   // Таймаут отправки данных
    config.global_user_ctx = NULL;
    config.global_user_ctx_free_fn = NULL;
    config.global_transport_ctx = NULL;
    config.global_transport_ctx_free_fn = NULL;
    config.open_fn = NULL;

    ESP_LOGI(TAG, "Starting WebSocket server on port %d", config.server_port);

    esp_err_t ret = httpd_start(&ws_server, &config);
    if (ret == ESP_OK)
    {
        httpd_uri_t ws_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = websocket_handler,
            .user_ctx = netif,
            .is_websocket = true};

        if (httpd_register_uri_handler(ws_server, &ws_uri) == ESP_OK)
        {
            ESP_LOGI(TAG, "WebSocket handler registered successfully");
            return;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to register WebSocket handler");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to start WebSocket server: %s", esp_err_to_name(ret));
    }

    // Cleanup on failure
    captive_portal_ws_server_stop();
}

//=================================================================
// Stop WebSocket server
//=================================================================
esp_err_t captive_portal_ws_server_stop(void)
{
    if (server_stopped)
        return ESP_OK;

    ESP_LOGI(TAG, "Stopping WebSocket server...");
    server_stopped = true;

    // Close client connection
    if (socket_mutex && ws_server)
    {
        if (xSemaphoreTake(socket_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            if (client_socket != -1)
            {
                httpd_sess_trigger_close(ws_server, client_socket);
                client_socket = -1;
            }
            xSemaphoreGive(socket_mutex);
        }
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    // Stop HTTP server
    if (ws_server)
    {
        httpd_stop(ws_server);
        ws_server = NULL;
    }

    // Clean up sender task
    if (sender_task_handle)
    {
        ws_msg_t stop_msg = {.payload = NULL, .len = 0};
        if (ws_send_queue)
        {
            xQueueSend(ws_send_queue, &stop_msg, pdMS_TO_TICKS(100));
        }

        for (int i = 0; i < 10 && sender_task_handle != NULL; i++)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (sender_task_handle)
        {
            vTaskDelete(sender_task_handle);
        }
        sender_task_handle = NULL;
    }

    // Clean up queue
    if (ws_send_queue)
    {
        ws_msg_t msg;
        while (xQueueReceive(ws_send_queue, &msg, 0) == pdTRUE)
        {
            if (msg.payload)
            {
                free(msg.payload);
            }
        }
        vQueueDelete(ws_send_queue);
        ws_send_queue = NULL;
    }

    // Clean up mutex
    if (socket_mutex)
    {
        vSemaphoreDelete(socket_mutex);
        socket_mutex = NULL;
    }

    ESP_LOGI(TAG, "WebSocket server stopped completely");
    return ESP_OK;
}

//=================================================================
// Send text message
//=================================================================
static bool ws_server_send_text(const char *data, size_t len)
{
    if (!ws_send_queue || !data || len == 0 || server_stopped)
        return false;

    char *payload = malloc(len + 1);
    if (!payload)
        return false;

    memcpy(payload, data, len);
    payload[len] = '\0';

    ws_msg_t msg = {.payload = payload, .len = len};

    if (xQueueSend(ws_send_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to enqueue message");
        free(payload);
        return false;
    }

    return true;
}

//=================================================================
// Send string message
//=================================================================
bool ws_server_send_string(const char *str)
{
    if (!str)
        return false;
    return ws_server_send_text(str, strlen(str));
}
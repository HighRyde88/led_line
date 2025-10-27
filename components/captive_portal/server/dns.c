#include "stdint.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "socket.h"
#include "esp_netif.h"

#define DNS_PORT 53
#define DNS_MAX_LEN 512
#define DNS_TASK_STACK_SIZE 4096
#define DNS_TASK_PRIORITY 5

static const char *TAG = "dns_server";

typedef struct __attribute__((packed))
{
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;

static TaskHandle_t dns_task_handler = NULL;
static esp_netif_t *dns_netif = NULL;
static EventGroupHandle_t dns_event_group = NULL;
static const int DNS_STOP_BIT = BIT0;

//=================================================================
static void dns_server_task(void *pvParameters)
{
    char rx_buffer[DNS_MAX_LEN];
    char tx_buffer[DNS_MAX_LEN];
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int len;
    esp_netif_t *netif = (esp_netif_t *)pvParameters;

    if (netif == NULL) {
        ESP_LOGE(TAG, "Invalid netif provided");
        vTaskDelete(NULL);
        return;
    }

    // Получить IP адрес интерфейса
    esp_netif_ip_info_t ip_info;
    esp_err_t err = esp_netif_get_ip_info(netif, &ip_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get IP info: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }
    
    uint32_t self_ip = ip_info.ip.addr;
    char ip_str[16];
    esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
    ESP_LOGI(TAG, "DNS server started on %s:%d", ip_str, DNS_PORT);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DNS_PORT);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        ESP_LOGE(TAG, "Failed to bind socket");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    // Установить таймаут для recvfrom для возможности прерывания
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    while (1) {
        // Проверить, нужно ли остановить сервер
        EventBits_t bits = xEventGroupGetBits(dns_event_group);
        if (bits & DNS_STOP_BIT) {
            break;
        }

        len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                       (struct sockaddr *)&client_addr, &client_len);
        
        if (len > 0) {
            if (len >= 12) {
                // Разобрать DNS заголовок
                dns_header_t *header = (dns_header_t *)rx_buffer;
                uint16_t dns_id = ntohs(header->id);
                uint16_t flags = ntohs(header->flags);
                
                // Проверить, что это запрос (не ответ)
                if ((flags & 0x8000) == 0) {
                    ESP_LOGD(TAG, "DNS query received, ID: 0x%04X", dns_id);

                    // Подготовить ответ
                    memset(tx_buffer, 0, sizeof(tx_buffer));
                    dns_header_t *resp_header = (dns_header_t *)tx_buffer;
                    
                    resp_header->id = htons(dns_id);
                    resp_header->flags = htons(0x8180); // Ответ, авторитетный
                    resp_header->qdcount = htons(1);    // Один вопрос
                    resp_header->ancount = htons(1);    // Один ответ
                    resp_header->nscount = htons(0);
                    resp_header->arcount = htons(0);

                    // Скопировать вопрос
                    int question_len = len - sizeof(dns_header_t);
                    if (question_len > 0 && question_len < (DNS_MAX_LEN - sizeof(dns_header_t) - 16)) {
                        memcpy(&tx_buffer[sizeof(dns_header_t)], 
                               &rx_buffer[sizeof(dns_header_t)], 
                               question_len);

                        // Добавить ответ (A запись)
                        int answer_offset = sizeof(dns_header_t) + question_len;
                        
                        // Указатель на имя в вопросе
                        tx_buffer[answer_offset++] = 0xC0;
                        tx_buffer[answer_offset++] = 0x0C;
                        
                        // Тип записи (A)
                        tx_buffer[answer_offset++] = 0x00;
                        tx_buffer[answer_offset++] = 0x01;
                        
                        // Класс (IN)
                        tx_buffer[answer_offset++] = 0x00;
                        tx_buffer[answer_offset++] = 0x01;
                        
                        // TTL (300 секунд)
                        tx_buffer[answer_offset++] = 0x00;
                        tx_buffer[answer_offset++] = 0x00;
                        tx_buffer[answer_offset++] = 0x01;
                        tx_buffer[answer_offset++] = 0x2C;
                        
                        // Длина данных (4 байта для IPv4)
                        tx_buffer[answer_offset++] = 0x00;
                        tx_buffer[answer_offset++] = 0x04;
                        
                        // IP адрес в формате big-endian
                        tx_buffer[answer_offset++] = (self_ip >> 0) & 0xFF;
                        tx_buffer[answer_offset++] = (self_ip >> 8) & 0xFF;
                        tx_buffer[answer_offset++] = (self_ip >> 16) & 0xFF;
                        tx_buffer[answer_offset++] = (self_ip >> 24) & 0xFF;

                        int response_len = answer_offset;

                        // Отправить ответ
                        sendto(sock, tx_buffer, response_len, 0,
                               (struct sockaddr *)&client_addr, client_len);
                        
                        ESP_LOGD(TAG, "DNS response sent to %s", inet_ntoa(client_addr.sin_addr));
                    }
                }
            }
        } else if (len < 0) {
            // Проверить errno для таймаута
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ESP_LOGW(TAG, "recvfrom error: %d", errno);
            }
        }
    }

    close(sock);
    ESP_LOGI(TAG, "DNS server stopped");
    vTaskDelete(NULL);
}

//=================================================================
esp_err_t captive_portal_dns_server_start(esp_netif_t *netif)
{
    if (netif == NULL) {
        ESP_LOGE(TAG, "Invalid netif provided");
        return ESP_ERR_INVALID_ARG;
    }

    if (dns_task_handler != NULL) {
        ESP_LOGW(TAG, "DNS server already running");
        return ESP_OK;
    }

    // Создать event group для управления задачей
    if (dns_event_group == NULL) {
        dns_event_group = xEventGroupCreate();
        if (dns_event_group == NULL) {
            ESP_LOGE(TAG, "Failed to create event group");
            return ESP_FAIL;
        }
    }
    
    // Сбросить биты
    xEventGroupClearBits(dns_event_group, DNS_STOP_BIT);

    dns_netif = netif;
    
    BaseType_t result = xTaskCreate(dns_server_task, "dns_server", 
                                   DNS_TASK_STACK_SIZE, netif, 
                                   DNS_TASK_PRIORITY, &dns_task_handler);
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create DNS server task");
        dns_task_handler = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "DNS server task created");
    return ESP_OK;
}

//=================================================================
esp_err_t captive_portal_dns_server_stop(void)
{
    if (dns_task_handler == NULL) {
        ESP_LOGW(TAG, "DNS server not running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping DNS server...");

    // Установить флаг остановки
    if (dns_event_group != NULL) {
        xEventGroupSetBits(dns_event_group, DNS_STOP_BIT);
    }

    // Подождать немного для корректной остановки
    vTaskDelay(pdMS_TO_TICKS(100));

    // Удалить задачу
    if (dns_task_handler != NULL) {
        vTaskDelete(dns_task_handler);
        dns_task_handler = NULL;
    }

    dns_netif = NULL;
    
    ESP_LOGI(TAG, "DNS server stopped");
    return ESP_OK;
}

//=================================================================
// Дополнительные вспомогательные функции
esp_err_t captive_portal_dns_server_is_running(void)
{
    return (dns_task_handler != NULL) ? ESP_OK : ESP_FAIL;
}

esp_netif_t* captive_portal_dns_get_netif(void)
{
    return dns_netif;
}
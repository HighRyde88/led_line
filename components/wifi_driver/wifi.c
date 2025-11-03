#include "wifi.h"
#include "string.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "dwnvs.h"
#include "mdns_service.h"
#include "esp_http_client.h"
#define DISCONNECT_BY_SCAN (BIT0)
#define DISCONNECT_BY_USER (BIT1)
#define SCAN_IN_PROGRESS_BIT (BIT2)
#define INTERNET_CHECK_TIMEOUT_MS 3000
#define INTERNET_CHECK_URL "http://connectivitycheck.gstatic.com/generate_204"
static uint16_t ap_count = 0;
static wifi_ap_record_t *ap_list = NULL;
static wifi_mode_t wifi_current_mode = WIFI_MODE_NULL;
static esp_event_handler_instance_t sta_wifi_event = NULL;
static esp_event_handler_instance_t sta_ip_event = NULL;
static esp_event_handler_instance_t sta_scan_event = NULL;
typedef esp_err_t (*connect_t)(void);
static connect_t connect_func = NULL;
static bool auto_reconnect_enabled = true; // Новая переменная для авто-переподключения
static esp_event_handler_t user_sta_event_handler = NULL;
static esp_event_handler_t user_scan_event_handler = NULL;
static SemaphoreHandle_t wifi_mutex = NULL;
static EventGroupHandle_t wifi_group = NULL;
static const char *TAG = "DWIFI";
static const char *TAGM = "DWIFI_M";
//=================================================================
static wifi_mode_t change_wifi_mode(wifi_mode_t mode, bool state)
{
    wifi_mode_t current_mode = WIFI_MODE_NULL;
    esp_err_t err = esp_wifi_get_mode(&current_mode);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get current Wi-Fi mode: %s", esp_err_to_name(err));
        return WIFI_MODE_NULL;
    }
    wifi_mode_t target_mode = current_mode;
    if (state == true)
    {
        switch (mode)
        {
        case WIFI_MODE_STA:
            if (current_mode == WIFI_MODE_NULL)
            {
                target_mode = WIFI_MODE_STA;
            }
            else if (current_mode == WIFI_MODE_AP)
            {
                target_mode = WIFI_MODE_APSTA;
            }
            break;
        case WIFI_MODE_AP:
            if (current_mode == WIFI_MODE_NULL)
            {
                target_mode = WIFI_MODE_AP;
            }
            else if (current_mode == WIFI_MODE_STA)
            {
                target_mode = WIFI_MODE_APSTA;
            }
            break;
        default:
            target_mode = WIFI_MODE_NULL;
            break;
        }
    }
    else
    {
        switch (mode)
        {
        case WIFI_MODE_STA:
            if (current_mode == WIFI_MODE_STA)
            {
                target_mode = WIFI_MODE_NULL;
            }
            else if (current_mode == WIFI_MODE_APSTA)
            {
                target_mode = WIFI_MODE_AP;
            }
            break;
        case WIFI_MODE_AP:
            if (current_mode == WIFI_MODE_AP)
            {
                target_mode = WIFI_MODE_NULL;
            }
            else if (current_mode == WIFI_MODE_APSTA)
            {
                target_mode = WIFI_MODE_STA;
            }
            break;
        default:
            target_mode = current_mode;
            break;
        }
    }
    ESP_LOGW(TAG, "Set new mode: %d", target_mode);
    return target_mode;
}
//=================================================================
void dw_set_user_sta_event_handler(esp_event_handler_t event)
{
    user_sta_event_handler = event;
}
//=================================================================
const char *wifi_reason_to_string(wifi_err_reason_t reason)
{
    switch (reason)
    {
    case WIFI_REASON_UNSPECIFIED:
        return "Unspecified reason";
    case WIFI_REASON_AUTH_EXPIRE:
        return "Authentication expired";
    case WIFI_REASON_AUTH_LEAVE:
        return "Deauthentication due to leaving";
    case WIFI_REASON_DISASSOC_DUE_TO_INACTIVITY:
        return "Disassociated due to inactivity";
    case WIFI_REASON_ASSOC_TOOMANY:
        return "Too many associated stations";
    case WIFI_REASON_CLASS2_FRAME_FROM_NONAUTH_STA:
        return "Class 2 frame received from nonauthenticated STA";
    case WIFI_REASON_CLASS3_FRAME_FROM_NONASSOC_STA:
        return "Class 3 frame received from nonassociated STA";
    case WIFI_REASON_ASSOC_LEAVE:
        return "Deassociated due to leaving";
    case WIFI_REASON_ASSOC_NOT_AUTHED:
        return "Association but not authenticated";
    case WIFI_REASON_DISASSOC_PWRCAP_BAD:
        return "Disassociated due to poor power capability";
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
        return "Disassociated due to unsupported channel";
    case WIFI_REASON_BSS_TRANSITION_DISASSOC:
        return "Disassociated due to BSS transition";
    case WIFI_REASON_IE_INVALID:
        return "Invalid Information Element (IE)";
    case WIFI_REASON_MIC_FAILURE:
        return "MIC failure";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        return "4-way handshake timeout";
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
        return "Group key update timeout";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:
        return "IE differs in 4-way handshake";
    case WIFI_REASON_GROUP_CIPHER_INVALID:
        return "Invalid group cipher";
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
        return "Invalid pairwise cipher";
    case WIFI_REASON_AKMP_INVALID:
        return "Invalid AKMP";
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
        return "Unsupported RSN IE version";
    case WIFI_REASON_INVALID_RSN_IE_CAP:
        return "Invalid RSN IE capabilities";
    case WIFI_REASON_802_1X_AUTH_FAILED:
        return "802.1X authentication failed";
    case WIFI_REASON_CIPHER_SUITE_REJECTED:
        return "Cipher suite rejected";
    case WIFI_REASON_TDLS_PEER_UNREACHABLE:
        return "TDLS peer unreachable";
    case WIFI_REASON_TDLS_UNSPECIFIED:
        return "TDLS unspecified";
    case WIFI_REASON_SSP_REQUESTED_DISASSOC:
        return "SSP requested disassociation";
    case WIFI_REASON_NO_SSP_ROAMING_AGREEMENT:
        return "No SSP roaming agreement";
    case WIFI_REASON_BAD_CIPHER_OR_AKM:
        return "Bad cipher or AKM";
    case WIFI_REASON_NOT_AUTHORIZED_THIS_LOCATION:
        return "Not authorized in this location";
    case WIFI_REASON_SERVICE_CHANGE_PERCLUDES_TS:
        return "Service change precludes TS";
    case WIFI_REASON_UNSPECIFIED_QOS:
        return "Unspecified QoS reason";
    case WIFI_REASON_NOT_ENOUGH_BANDWIDTH:
        return "Not enough bandwidth";
    case WIFI_REASON_MISSING_ACKS:
        return "Missing ACKs";
    case WIFI_REASON_EXCEEDED_TXOP:
        return "Exceeded TXOP";
    case WIFI_REASON_STA_LEAVING:
        return "Station leaving";
    case WIFI_REASON_END_BA:
        return "End of Block Ack (BA)";
    case WIFI_REASON_UNKNOWN_BA:
        return "Unknown Block Ack (BA)";
    case WIFI_REASON_TIMEOUT:
        return "Timeout";
    case WIFI_REASON_PEER_INITIATED:
        return "Peer initiated disassociation";
    case WIFI_REASON_AP_INITIATED:
        return "AP initiated disassociation";
    case WIFI_REASON_INVALID_FT_ACTION_FRAME_COUNT:
        return "Invalid FT action frame count";
    case WIFI_REASON_INVALID_PMKID:
        return "Invalid PMKID";
    case WIFI_REASON_INVALID_MDE:
        return "Invalid MDE";
    case WIFI_REASON_INVALID_FTE:
        return "Invalid FTE";
    case WIFI_REASON_TRANSMISSION_LINK_ESTABLISH_FAILED:
        return "Transmission link establishment failed";
    case WIFI_REASON_ALTERATIVE_CHANNEL_OCCUPIED:
        return "Alternative channel occupied";
    // Специфичные для ESP32 причины
    case WIFI_REASON_BEACON_TIMEOUT:
        return "Beacon timeout";
    case WIFI_REASON_NO_AP_FOUND:
        return "No AP found";
    case WIFI_REASON_AUTH_FAIL:
        return "Authentication failed";
    case WIFI_REASON_ASSOC_FAIL:
        return "Association failed";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "Handshake timeout";
    case WIFI_REASON_CONNECTION_FAIL:
        return "Connection failed";
    case WIFI_REASON_AP_TSF_RESET:
        return "AP TSF reset";
    case WIFI_REASON_ROAMING:
        return "Roaming";
    case WIFI_REASON_ASSOC_COMEBACK_TIME_TOO_LONG:
        return "Association comeback time too long";
    case WIFI_REASON_SA_QUERY_TIMEOUT:
        return "SA query timeout";
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
        return "No AP found with compatible security";
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
        return "No AP found in auth mode threshold";
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
        return "No AP found in RSSI threshold";
    default:
        return "Unknown reason";
    }
}
//=================================================================
static bool wifi_mutex_lock(TickType_t timeout)
{
    if (wifi_mutex == NULL)
    {
        ESP_LOGE(TAGM, "WiFi mutex is NULL, cannot lock");
        return false;
    }
    ESP_LOGW(TAGM, "Attempting to lock WiFi mutex (timeout: %lu ms)", timeout * portTICK_PERIOD_MS);
    BaseType_t result = xSemaphoreTake(wifi_mutex, timeout);
    if (result == pdTRUE)
    {
        ESP_LOGW(TAGM, "WiFi mutex locked successfully");
        return true;
    }
    else
    {
        if (timeout == 0)
        {
            ESP_LOGW(TAGM, "Failed to lock WiFi mutex (would block)");
        }
        else
        {
            ESP_LOGE(TAGM, "Failed to lock WiFi mutex (timeout: %lu ms)", timeout * portTICK_PERIOD_MS);
        }
        return false;
    }
}
//=================================================================
static void wifi_mutex_unlock(void)
{
    if (wifi_mutex == NULL)
    {
        ESP_LOGE(TAGM, "WiFi mutex is NULL, cannot unlock");
        return;
    }
    BaseType_t result = xSemaphoreGive(wifi_mutex);
    if (result == pdTRUE)
    {
        ESP_LOGW(TAGM, "WiFi mutex unlocked successfully");
    }
    else
    {
        ESP_LOGE(TAGM, "Failed to unlock WiFi mutex - not owned by this task or invalid state");
    }
}
//=================================================================
esp_err_t dw_resources_init(void)
{
    // Инициализация сетевого стека
    ESP_ERROR_CHECK(esp_netif_init());
    // Инициализация Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
#ifdef DWNVS_STORAGE_H
    dwnvs_storage_initialization();
#endif
    // Создание сетевых интерфейсов
    if (esp_netif_get_handle_from_ifkey("WIFI_STA_DEF") == NULL)
    {
        esp_netif_create_default_wifi_sta();
        if (esp_netif_get_handle_from_ifkey("WIFI_STA_DEF") == NULL)
        {
            ESP_LOGE(TAG, "Failed to create STA netif");
            return ESP_ERR_NO_MEM;
        }
    }
    if (esp_netif_get_handle_from_ifkey("WIFI_AP_DEF") == NULL)
    {
        esp_netif_create_default_wifi_ap();
        if (esp_netif_get_handle_from_ifkey("WIFI_AP_DEF") == NULL)
        {
            ESP_LOGE(TAG, "Failed to create AP netif");
            return ESP_ERR_NO_MEM;
        }
    }
    // Создание синхронизации примитивов
    if (wifi_mutex == NULL)
    {
        wifi_mutex = xSemaphoreCreateMutex();
        if (wifi_mutex == NULL)
        {
            ESP_LOGE(TAG, "Failed to create wifi mutex");
            return ESP_ERR_NO_MEM;
        }
    }
    if (wifi_group == NULL)
    {
        wifi_group = xEventGroupCreate();
        if (wifi_group == NULL)
        {
            ESP_LOGE(TAG, "Failed to create wifi event group");
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}
//=================================================================
void dw_wifi_cleanup(void)
{
    // Попытка захвата мьютекса с увеличенным таймаутом
    if (!wifi_mutex_lock(pdMS_TO_TICKS(5000)))
    {
        ESP_LOGE(TAG, "CRITICAL: Failed to acquire WiFi mutex in cleanup - potential memory leak!");
        // Принудительная очистка без мьютекса (рискованно, но лучше чем ничего)
        if (ap_list)
        {
            free(ap_list);
            ap_list = NULL;
        }
        ap_count = 0;
        // Если мьютекс не удалось захватить, но он создан, нужно его удалить
        // ВАЖНО: Это может быть небезопасно, если другой поток всё ещё использует его.
        // Предполагаем, что в момент очистки других активных потоков нет.
        if (wifi_mutex)
        {
            vSemaphoreDelete(wifi_mutex);
            wifi_mutex = NULL; // Убедимся, что указатель обнулен
        }
        return;
    }

    // Безопасная очистка с мьютексом
    if (ap_list)
    {
        free(ap_list);
        ap_list = NULL;
        ap_count = 0;
    }
    // Удаляем мьютекс только после очистки других ресурсов
    if (wifi_mutex)
    {
        SemaphoreHandle_t temp_mutex = wifi_mutex;
        wifi_mutex = NULL; // Помечаем как недоступный до удаления
        vSemaphoreDelete(temp_mutex);
    }
    // Мьютекс уже захвачен, но если он был удалён выше, unlock может вызвать ошибку.
    // Проверим, не стал ли wifi_mutex NULL.
    if (wifi_mutex != NULL)
    {
        wifi_mutex_unlock(); // Разблокируем мьютекс, если он не был удалён
    }
    // Группа событий не удаляется, так как она может использоваться в других местах
    // или её удаление может вызвать проблемы в других задачах.
    // Обычно группы событий удаляются только при уверенности, что никто больше не использует.
    // Для простоты в этом примере она не удаляется.
}
//=================================================================
esp_err_t dw_access_point_start(const wifi_ap_config_t *ap_config)
{
    if (!wifi_mutex_lock(pdMS_TO_TICKS(1000)))
    {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t result = ESP_OK;
    if (ap_config == NULL)
    {
        ESP_LOGE(TAG, "AP config is NULL");
        result = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    if (ap_config->ssid[0] == '\0')
    {
        ESP_LOGE(TAG, "AP SSID is empty");
        result = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    wifi_config_t wifi_config = {0};
    memcpy(&wifi_config.ap, ap_config, sizeof(wifi_ap_config_t));
    if (ap_config->password[0] == '\0')
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    else
    {
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        size_t pwd_len = strlen((const char *)ap_config->password);
        if (pwd_len < 8 || pwd_len > 64)
        {
            ESP_LOGE(TAG, "AP password must be 8..64 characters");
            result = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
    }
    if (wifi_config.ap.ssid_len == 0)
    {
        wifi_config.ap.ssid_len = strnlen((const char *)wifi_config.ap.ssid, 32);
    }
    wifi_current_mode = change_wifi_mode(WIFI_MODE_AP, true);
    esp_err_t err = esp_wifi_set_mode(wifi_current_mode);
    if (err != ESP_OK)
    {
        result = err;
        goto cleanup;
    }
    err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (err != ESP_OK)
    {
        result = err;
        goto cleanup;
    }
    if (wifi_current_mode != WIFI_MODE_NULL)
    {
        err = esp_wifi_start();
        if (err != ESP_OK)
        {
            result = err;
            goto cleanup;
        }
    }
cleanup:
    wifi_mutex_unlock();
    return result;
}
//=================================================================
esp_err_t dw_access_point_stop(void)
{
    esp_err_t result = ESP_OK;
    esp_err_t err;
    wifi_mode_t target_mode = WIFI_MODE_NULL;
    if (!wifi_mutex_lock(pdMS_TO_TICKS(1000)))
    {
        return ESP_ERR_TIMEOUT;
    }
    err = esp_wifi_get_mode(&wifi_current_mode);
    if (err != ESP_OK)
    {
        result = err;
        wifi_mutex_unlock();
        return result;
    }
    if (wifi_current_mode != WIFI_MODE_AP && wifi_current_mode != WIFI_MODE_APSTA)
    {
        wifi_mutex_unlock();
        return result;
    }
    target_mode = change_wifi_mode(WIFI_MODE_AP, false);
    // Выполняем потенциально блокирующие операции внутри критической секции
    // Отключаем всех клиентов
    err = esp_wifi_deauth_sta(0);
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED)
    {
        ESP_LOGW(TAG, "Failed to deauth clients: %s", esp_err_to_name(err));
        // Не возвращаем ошибку, так как основная цель - остановить AP
    }
    err = esp_wifi_set_mode(target_mode);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(err));
        result = err; // Сохраняем ошибку
    }
    if (target_mode == WIFI_MODE_NULL)
    {
        err = esp_wifi_stop();
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED)
        {
            ESP_LOGE(TAG, "Failed to stop WiFi: %s", esp_err_to_name(err));
            if (result == ESP_OK)
            { // Если предыдущая операция была успешна, сохраняем ошибку остановки
                result = err;
            }
        }
    }
    wifi_mutex_unlock();
    return result;
}
//=================================================================
static void dw_sta_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI(TAG, "Got IP");
        // Проверяем, доступен ли мьютекс
        if (wifi_mutex != NULL)
        {
            if (wifi_mutex_lock(pdMS_TO_TICKS(1000)))
            {
#ifdef DWNVS_STORAGE_H
                wifi_config_t wifi_config = {0};
                esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
                if (err == ESP_OK)
                {
                    ESP_LOGI(TAG, "Wi-Fi config retrieved successfully");
                    if (wifi_config.sta.ssid[0] != '\0')
                    {
                        // Сохраняем только STA-часть
                        wifi_sta_config_t wifi_sta_config = {0};
                        memcpy(&wifi_sta_config, &wifi_config.sta, sizeof(wifi_sta_config));
                        err = dwnvs_save_sta_config(&wifi_sta_config);
                        if (err == ESP_OK)
                        {
                            ESP_LOGI(TAG, "Wi-Fi STA config saved to NVS");
                        }
                        else
                        {
                            ESP_LOGE(TAG, "Failed to save Wi-Fi config to NVS: %s", esp_err_to_name(err));
                        }
                    }
                    else
                    {
                        ESP_LOGW(TAG, "No SSID configured in Wi-Fi settings");
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to get Wi-Fi config: %s", esp_err_to_name(err));
                }
#endif
                connect_func = &esp_wifi_connect;
                wifi_mutex_unlock();
            }
            else
            {
                ESP_LOGE(TAG, "Failed to acquire WiFi mutex within 1 second");
            }
        }
        else
        {
            ESP_LOGW(TAG, "WiFi mutex is NULL in IP event handler");
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI(TAG, "Connected.");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "Disconnected. Reason: %s (code: %d)", wifi_reason_to_string(disconn->reason), disconn->reason);
        if (disconn->reason == WIFI_REASON_ASSOC_LEAVE)
        {
            // Явное отключение пользователем
            if (wifi_mutex != NULL && wifi_mutex_lock(pdMS_TO_TICKS(1000)))
            {
                EventBits_t bits = xEventGroupWaitBits(wifi_group, DISCONNECT_BY_USER, pdTRUE, pdTRUE, 0);
                if (bits & DISCONNECT_BY_USER)
                {
                    ESP_LOGI(TAG, "User-initiated disconnect - cleaning up WiFi");
                    wifi_mode_t target_mode = change_wifi_mode(WIFI_MODE_STA, false);
                    if (sta_wifi_event)
                    {
                        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, sta_wifi_event);
                        sta_wifi_event = NULL;
                    }
                    if (sta_ip_event)
                    {
                        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, sta_ip_event);
                        sta_ip_event = NULL;
                    }
                    esp_err_t err = esp_wifi_set_mode(target_mode);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(err));
                    }
                    if (target_mode == WIFI_MODE_NULL)
                    {
                        err = esp_wifi_stop();
                        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED)
                        {
                            ESP_LOGE(TAG, "Failed to stop WiFi: %s", esp_err_to_name(err));
                        }
                    }
                    // Явное отключение отключает авто-переподключение
                    auto_reconnect_enabled = false;
                    connect_func = NULL;
                }
                wifi_mutex_unlock();
            }
        }
        else
        {
            // Автоматическое отключение - решаем, переподключаться ли
            if (wifi_mutex != NULL && wifi_mutex_lock(pdMS_TO_TICKS(1000)))
            {
                bool should_reconnect = false;
                uint32_t reconnect_delay = 0;
                // Проверяем, включено ли авто-переподключение
                if (!auto_reconnect_enabled)
                {
                    ESP_LOGI(TAG, "Auto-reconnect is disabled, not attempting to reconnect.");
                    connect_func = NULL; // Убедимся, что переподключение не произойдёт
                }
                else
                {
                    switch (disconn->reason)
                    {
                        // Ошибки аутентификации - не переподключаемся автоматически
                    case WIFI_REASON_AUTH_EXPIRE:             // 2
                    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:  // 15
                    case WIFI_REASON_AUTH_FAIL:               // 202
                    case WIFI_REASON_802_1X_AUTH_FAILED:      // 23
                    case WIFI_REASON_AKMP_INVALID:            // 20
                    case WIFI_REASON_PAIRWISE_CIPHER_INVALID: // 19
                    case WIFI_REASON_GROUP_CIPHER_INVALID:    // 18
                    case WIFI_REASON_BAD_CIPHER_OR_AKM:       // 29
                    case WIFI_REASON_INVALID_PMKID:           // 49
                    case WIFI_REASON_IE_IN_4WAY_DIFFERS:      // 17
                        ESP_LOGE(TAG, "Authentication failed - manual intervention required");
                        connect_func = NULL;
                        auto_reconnect_enabled = false; // Отключаем на всякий случай
                        break;
                    // Точка не найдена - не переподключаемся
                    case WIFI_REASON_NO_AP_FOUND:                       // 201
                    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY: // 210
                    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD: // 211
                    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:     // 212
                        ESP_LOGW(TAG, "AP not found - will retry when AP becomes available");
                        // Все равно пытаемся переподключиться через некоторое время
                        should_reconnect = true;
                        reconnect_delay = 2000; // 5 секунд для "AP not found"
                        break;
                    // Таймауты - переподключаемся немедленно
                    case WIFI_REASON_HANDSHAKE_TIMEOUT:        // 204
                    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: // 16
                    case WIFI_REASON_SA_QUERY_TIMEOUT:         // 209
                        ESP_LOGI(TAG, "Handshake timeout - immediate reconnect");
                        should_reconnect = true;
                        reconnect_delay = 0;
                        break;
                    // Потеря сигнала - переподключаемся с небольшой задержкой
                    case WIFI_REASON_BEACON_TIMEOUT: // 200
                    case WIFI_REASON_TIMEOUT:        // 39
                        ESP_LOGI(TAG, "Signal lost - reconnect with delay");
                        should_reconnect = true;
                        reconnect_delay = 1000; // 1 секунда
                        break;
                    // Роуминг и временные проблемы
                    case WIFI_REASON_ROAMING:                 // 207
                    case WIFI_REASON_BSS_TRANSITION_DISASSOC: // 12
                    case WIFI_REASON_PEER_INITIATED:          // 46
                    case WIFI_REASON_AP_INITIATED:            // 47
                    case WIFI_REASON_AP_TSF_RESET:            // 206
                        ESP_LOGI(TAG, "Temporary disconnection - reconnect with delay");
                        should_reconnect = true;
                        reconnect_delay = 1000; // 2 секунды
                        break;
                    // Перегрузка AP
                    case WIFI_REASON_ASSOC_TOOMANY:        // 5
                    case WIFI_REASON_NOT_ENOUGH_BANDWIDTH: // 33
                        ESP_LOGI(TAG, "AP overloaded - reconnect with exponential delay");
                        should_reconnect = true;
                        reconnect_delay = 2000; // 5 секунд
                        break;
                    // Неопределенные причины
                    case WIFI_REASON_UNSPECIFIED:                // 1
                    case WIFI_REASON_DISASSOC_DUE_TO_INACTIVITY: // 4
                    case WIFI_REASON_CONNECTION_FAIL:            // 205
                    default:
                        ESP_LOGW(TAG, "Unexpected disconnect reason %d - reconnect with delay", disconn->reason);
                        should_reconnect = true;
                        reconnect_delay = 2000; // 3 секунды по умолчанию
                        break;
                    }
                    // Выполняем переподключение если нужно
                    if (should_reconnect && connect_func)
                    {
                        if (reconnect_delay > 0)
                        {
                            ESP_LOGI(TAG, "Scheduling reconnect in %lu ms", reconnect_delay);
                            // ВАЖНО: Захват мьютекса не должен быть активен во время vTaskDelay!
                            // Поэтому разблокируем его перед задержкой и захватим снова после.
                            wifi_mutex_unlock();
                            vTaskDelay(pdMS_TO_TICKS(reconnect_delay));
                            if (!wifi_mutex_lock(pdMS_TO_TICKS(1000)))
                            {
                                ESP_LOGE(TAG, "Failed to re-acquire WiFi mutex after delay for reconnect");
                                return; // Выходим, если не удалось захватить мьютекс снова
                            }
                        }
                        ESP_LOGI(TAG, "Attempting to reconnect...");
                        esp_err_t err = connect_func();

                        arg = (void *)&should_reconnect;

                        if (err != ESP_OK)
                        {
                            ESP_LOGE(TAG, "Reconnect failed: %s", esp_err_to_name(err));
                        }
                    }
                }
                wifi_mutex_unlock();
            }
            else
            {
                ESP_LOGW(TAG, "Failed to acquire WiFi mutex in disconnect handler");
            }
        }
    }
    if (user_sta_event_handler)
    {
        user_sta_event_handler(arg, event_base, event_id, event_data);
    }
}
//=================================================================
esp_err_t dw_station_connect_with_auto_reconnect(const wifi_sta_config_t *sta_config, esp_event_handler_t event, void *arg, bool auto_reconnect)
{
    if (!wifi_mutex_lock(pdMS_TO_TICKS(1000)))
    {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t result = ESP_OK;
    esp_err_t err = ESP_OK;
    esp_netif_ip_info_t ip_info = {0};
    esp_netif_t *wifi_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (wifi_netif)
    {
#ifdef DWNVS_STORAGE_H
        err = dwnvs_load_ipinfo("network", &ip_info);
        if (err == ESP_OK)
        {
            // Останавливаем DHCP клиент и устанавливаем статический IP
            esp_netif_dhcpc_stop(wifi_netif); // Исправлено: dhcpc_stop вместо dhcps_stop
            esp_netif_set_ip_info(wifi_netif, &ip_info);
            ESP_LOGI(TAG, "Static IP configured");
        }
        else
        {
            // Запускаем DHCP клиент
            esp_netif_dhcpc_start(wifi_netif); // Исправлено: dhcpc_start
            ESP_LOGI(TAG, "DHCP client started");
        }
#elif
        esp_netif_dhcpc_start(wifi_netif); // Исправлено: dhcpc_start
        ESP_LOGI(TAG, "DHCP client started");
#endif
    }
    // Отменяем регистрацию предыдущих обработчиков событий
    if (sta_wifi_event != NULL)
    {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, sta_wifi_event);
        sta_wifi_event = NULL;
    }
    if (sta_ip_event != NULL)
    {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, sta_ip_event);
        sta_ip_event = NULL;
    }
    // Регистрируем обработчики событий
    err = esp_event_handler_instance_register(WIFI_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              &dw_sta_event_handler,
                                              arg,
                                              &sta_wifi_event);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(err));
        result = err;
        goto cleanup;
    }
    err = esp_event_handler_instance_register(IP_EVENT,
                                              IP_EVENT_STA_GOT_IP,
                                              &dw_sta_event_handler,
                                              arg,
                                              &sta_ip_event);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(err));
        result = err;
        goto cleanup;
    }
    // Устанавливаем режим Wi-Fi
    wifi_current_mode = change_wifi_mode(WIFI_MODE_STA, true);
    err = esp_wifi_set_mode(wifi_current_mode);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(err));
        result = err;
        goto cleanup;
    }
    // Настраиваем конфигурацию Wi-Fi
    wifi_config_t wifi_config = {0};
    if (sta_config != NULL)
    {
        memcpy(&wifi_config.sta, sta_config, sizeof(wifi_sta_config_t));
    }
    else
    {
#ifdef DWNVS_STORAGE_H
        // Загружаем сохраненную конфигурацию
        err = dwnvs_load_sta_config(&wifi_config.sta);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "No saved STA config found, using default settings");
            wifi_config.sta.scan_method = WIFI_FAST_SCAN;
            wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
            wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
            wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
            wifi_config.sta.failure_retry_cnt = 3;
        }
#elif
        return ESP_ERR_INVALID_ARG; // Ошибка, если конфигурация не указана и dwnvs не используется
#endif
    }
    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(err));
        result = err;
        goto cleanup;
    }
    // Сохраняем пользовательский обработчик событий
    user_sta_event_handler = event;
    connect_func = &esp_wifi_connect;
    // Устанавливаем флаг авто-переподключения
    auto_reconnect_enabled = auto_reconnect;
    // Запускаем Wi-Fi
    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED)
    {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(err));
        result = err;
        goto cleanup;
    }
    // Проверяем текущее соединение
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
    {
        // Уже подключены, отключаемся перед новым подключением
        ESP_LOGI(TAG, "Already connected, disconnecting...");
        result = esp_wifi_disconnect();
        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "Reconnecting to SSID: %.*s", 32, wifi_config.sta.ssid);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to disconnect: %s", esp_err_to_name(result));
        }
    }
    else
    {
        // Подключаемся к точке доступа
        result = esp_wifi_connect();
        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "Connecting to SSID: %.*s", 32, wifi_config.sta.ssid);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to start connection to SSID: %.*s, error: %s",
                     32, wifi_config.sta.ssid, esp_err_to_name(result));
        }
    }
cleanup:
    wifi_mutex_unlock();
    return result;
}
//=================================================================
esp_err_t dw_station_connect(const wifi_sta_config_t *sta_config, esp_event_handler_t event, void *arg)
{
    return dw_station_connect_with_auto_reconnect(sta_config, event, arg, true);
}
//=================================================================
esp_err_t dw_station_stop(void)
{
    if (!wifi_mutex_lock(pdMS_TO_TICKS(1000)))
    {
        return ESP_ERR_TIMEOUT;
    }
    esp_netif_ip_info_t ip_info;
    esp_netif_t *esp_sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (esp_sta_netif && esp_netif_get_ip_info(esp_sta_netif, &ip_info) == ESP_OK)
    {
        ESP_LOGD(TAG, "Disconnecting WiFi...");
        // Устанавливаем флаг до вызова esp_wifi_disconnect
        xEventGroupSetBits(wifi_group, DISCONNECT_BY_USER);
        // Отключаем авто-переподключение
        auto_reconnect_enabled = false;
        connect_func = NULL; // Очищаем функцию подключения
        wifi_mutex_unlock(); // Разблокируем мьютекс перед вызовом esp_wifi_disconnect
        return esp_wifi_disconnect();
    }
    wifi_mutex_unlock();
    return ESP_ERR_NOT_FOUND;
}
//=================================================================
static void ds_scan_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE)
    {
        wifi_event_sta_scan_done_t *scan_done_event = (wifi_event_sta_scan_done_t *)event_data;
        if (scan_done_event->status == 0)
        {
            // Проверяем, доступен ли мьютекс
            if (wifi_mutex != NULL)
            {
                if (wifi_mutex_lock(pdMS_TO_TICKS(1000)))
                {
                    if (ap_list)
                    {
                        free(ap_list);
                        ap_list = NULL;
                    }
                    ap_count = 0;
                    ap_count = scan_done_event->number;
                    if (ap_count > 0)
                    {
                        ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
                        if (ap_list)
                        {
                            esp_wifi_scan_get_ap_records(&ap_count, ap_list);
                            ESP_LOGI(TAG, "Scan done: %d APs found", ap_count);
                        }
                        else
                        {
                            ESP_LOGE(TAG, "Failed to allocate memory for AP list");
                            ap_count = 0;
                        }
                    }
                    // СБРОС ФЛАГА СКАНИРОВАНИЯ
                    xEventGroupClearBits(wifi_group, SCAN_IN_PROGRESS_BIT);
                    wifi_mutex_unlock();
                }
                else
                {
                    ESP_LOGW(TAG, "Failed to acquire WiFi mutex in scan done handler");
                }
            }
            else
            {
                ESP_LOGW(TAG, "WiFi mutex is NULL in scan done handler");
            }
        }
        else
        {
            ESP_LOGE(TAG, "Scan failed");
            // ТАКЖЕ СБРАСЫВАЕМ ФЛАГ В СЛУЧАЕ ОШИБКИ
            if (wifi_mutex != NULL && wifi_mutex_lock(pdMS_TO_TICKS(1000)))
            {
                xEventGroupClearBits(wifi_group, SCAN_IN_PROGRESS_BIT);
                wifi_mutex_unlock();
            }
        }
    }
    if (user_scan_event_handler)
    {
        user_scan_event_handler(arg, event_base, event_id, event_data);
    }
}
//=================================================================
esp_err_t dw_station_scan_start(esp_event_handler_t event, void *arg)
{
    wifi_mode_t target_mode;
    esp_event_handler_t saved_user_handler = event;
    if (!wifi_mutex_lock(pdMS_TO_TICKS(1000)))
    {
        return ESP_ERR_TIMEOUT;
    }
    // ПРОВЕРКА: активно ли уже сканирование?
    if (xEventGroupGetBits(wifi_group) & SCAN_IN_PROGRESS_BIT)
    {
        ESP_LOGW(TAG, "Scan already in progress, ignoring new scan request");
        wifi_mutex_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    // Устанавливаем флаг сканирования
    xEventGroupSetBits(wifi_group, SCAN_IN_PROGRESS_BIT);
    // Подготовка данных внутри критической секции
    if (ap_list)
    {
        free(ap_list);
        ap_list = NULL;
        ap_count = 0;
    }
    target_mode = change_wifi_mode(WIFI_MODE_STA, true);
    // Сохраняем обработчики событий
    user_scan_event_handler = saved_user_handler;
    // Выполняем потенциально блокирующие операции внутри критической секции
    esp_wifi_scan_stop();
    esp_netif_ip_info_t ip_info;
    esp_netif_t *esp_sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (esp_sta_netif && esp_netif_get_ip_info(esp_sta_netif, &ip_info) == ESP_OK)
    {
        ESP_LOGD(TAG, "Disconnecting WiFi...");
        esp_wifi_disconnect();
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    esp_err_t err = esp_wifi_set_mode(target_mode);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(err));
        // СБРОС ФЛАГА В СЛУЧАЕ ОШИБКИ
        xEventGroupClearBits(wifi_group, SCAN_IN_PROGRESS_BIT);
        wifi_mutex_unlock();
        return err;
    }
    // Регистрация обработчиков внутри критической секции
    if (sta_scan_event != NULL)
    {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, sta_scan_event);
        sta_scan_event = NULL;
    }
    err = esp_event_handler_instance_register(WIFI_EVENT,
                                              WIFI_EVENT_SCAN_DONE,
                                              &ds_scan_event_handler,
                                              arg,
                                              &sta_scan_event);
    if (err != ESP_OK)
    {
        // СБРОС ФЛАГА В СЛУЧАЕ ОШИБКИ
        xEventGroupClearBits(wifi_group, SCAN_IN_PROGRESS_BIT);
        wifi_mutex_unlock();
        return err;
    }
    err = esp_wifi_clear_ap_list();
    if (err != ESP_OK)
    {
        // СБРОС ФЛАГА В СЛУЧАЕ ОШИБКИ
        xEventGroupClearBits(wifi_group, SCAN_IN_PROGRESS_BIT);
        wifi_mutex_unlock();
        return err;
    }
    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED)
    {
        // СБРОС ФЛАГА В СЛУЧАЕ ОШИБКИ
        xEventGroupClearBits(wifi_group, SCAN_IN_PROGRESS_BIT);
        wifi_mutex_unlock();
        return err;
    }
    /*wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,
                .max = 300}}};
    */

    wifi_scan_config_t scan_config = WIFI_SCAN_PARAMS_DEFAULT_CONFIG();

    err = esp_wifi_scan_start(&scan_config, false);
    // Если сканирование не удалось запустить, сбрасываем флаг
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start scan: %s", esp_err_to_name(err));
        xEventGroupClearBits(wifi_group, SCAN_IN_PROGRESS_BIT);
    }
    wifi_mutex_unlock();
    return err;
}
//=================================================================
esp_err_t dw_station_scan_result(uint16_t *out_count, wifi_ap_record_t **out_list)
{
    if (out_count == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    // Проверяем, доступен ли мьютекс
    if (wifi_mutex == NULL)
    {
        ESP_LOGE(TAG, "WiFi mutex is NULL in scan result");
        return ESP_ERR_INVALID_STATE;
    }
    if (!wifi_mutex_lock(pdMS_TO_TICKS(1000)))
    {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t result = ESP_OK;
    if (ap_list == NULL || ap_count == 0)
    {
        *out_count = 0;
        if (out_list)
            *out_list = NULL;
        goto cleanup;
    }
    *out_count = ap_count;
    if (out_list)
    {
        size_t size = sizeof(wifi_ap_record_t) * ap_count;
        wifi_ap_record_t *copy = malloc(size);
        if (!copy)
        {
            result = ESP_ERR_NO_MEM;
            *out_list = NULL;
        }
        else
        {
            memcpy(copy, ap_list, size);
            *out_list = copy;
        }
    }
cleanup:
    wifi_mutex_unlock();
    return result;
}
//=================================================================
esp_err_t dw_free_scan_result(wifi_ap_record_t *scan_list)
{
    if (scan_list == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    free(scan_list);
    return ESP_OK;
}
//=================================================================
esp_err_t dw_set_hostname_to_netif(wifi_interface_t wifi_interface, const char *hostname)
{
    // Проверка входных параметров
    if (hostname == NULL)
    {
        ESP_LOGE(TAG, "hostname is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Проверка длины hostname (обычно до 63 символов)
    size_t hostname_len = strlen(hostname);
    if (hostname_len == 0 || hostname_len > 63)
    {
        ESP_LOGE(TAG, "Invalid hostname length: %d (must be 1-63)", (int)hostname_len);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Setting hostname '%s' for interface %s", hostname,
             wifi_interface == WIFI_IF_STA ? "STA" : "AP");

    // Получение сетевого интерфейса
    esp_netif_t *wifi_netif = NULL;
    if (wifi_interface == WIFI_IF_STA)
    {
        wifi_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        ESP_LOGD(TAG, "Got STA netif handle: %p", wifi_netif);
    }
    else if (wifi_interface == WIFI_IF_AP)
    {
        wifi_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        ESP_LOGD(TAG, "Got AP netif handle: %p", wifi_netif);
    }
    else
    {
        ESP_LOGE(TAG, "Invalid wifi_interface: %d", wifi_interface);
        return ESP_ERR_INVALID_ARG;
    }

    if (wifi_netif)
    {
        // Установка hostname для сетевого интерфейса
        esp_err_t err = esp_netif_set_hostname(wifi_netif, hostname);
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "Hostname '%s' successfully set for interface", hostname);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to set hostname '%s', error: %s", hostname, esp_err_to_name(err));
            return err; // возвращаем ошибку от esp_netif_set_hostname
        }

#ifdef MDNS_SERVICE_H
        ESP_LOGI(TAG, "Initializing mDNS service with hostname: %s", hostname);
        esp_err_t mdns_err = init_mdns_service(hostname, "Esp32Device");
        if (mdns_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize mDNS service, error: %s", esp_err_to_name(mdns_err));
            // Не возвращаем ошибку от mDNS, т.к. hostname уже установлен
        }
        else
        {
            ESP_LOGI(TAG, "mDNS service initialized successfully");
        }
#endif

        return err; // возвращаем результат от esp_netif_set_hostname
    }
    else
    {
        ESP_LOGE(TAG, "Failed to get netif handle for interface %s",
                 wifi_interface == WIFI_IF_STA ? "STA" : "AP");
        return ESP_ERR_NOT_FOUND;
    }
}
//=================================================================
esp_err_t dw_check_internet_connection(void)
{
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta_netif)
    {
        ESP_LOGE(TAG, "STA netif not found");
        return ESP_ERR_NOT_FOUND;
    }
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(sta_netif, &ip_info) != ESP_OK || ip_info.ip.addr == 0)
    {
        ESP_LOGW(TAG, "No valid IP address assigned");
        return ESP_ERR_INVALID_STATE;
    }
    esp_http_client_config_t config = {
        .url = INTERNET_CHECK_URL,
        .method = HTTP_METHOD_GET,
        .timeout_ms = INTERNET_CHECK_TIMEOUT_MS,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (err == ESP_OK && status_code == 204)
    {
        ESP_LOGI(TAG, "✅ Internet connection verified");
        return ESP_OK;
    }
    else
    {
        ESP_LOGW(TAG, "❌ Internet check failed: HTTP %d, err %s", status_code, esp_err_to_name(err));
        return ESP_ERR_INVALID_RESPONSE;
    }
}
//=================================================================
bool dw_is_scanning(void)
{
    EventBits_t bits = xEventGroupGetBits(wifi_group);
    return (bits & SCAN_IN_PROGRESS_BIT) != 0;
}
//=================================================================
void dw_force_scan_reset(void)
{
    xEventGroupClearBits(wifi_group, SCAN_IN_PROGRESS_BIT);
}
//=================================================================
void dw_station_set_auto_reconnect(bool enable)
{
    if (wifi_mutex != NULL && wifi_mutex_lock(pdMS_TO_TICKS(1000)))
    {
        auto_reconnect_enabled = enable;
        if (!enable)
        {
            connect_func = NULL;
        }
        ESP_LOGI(TAG, "Auto-reconnect %s", enable ? "enabled" : "disabled");
        wifi_mutex_unlock();
    }
    else
    {
        ESP_LOGE(TAG, "Failed to acquire WiFi mutex to set auto-reconnect state");
    }
}
//=================================================================
esp_err_t dw_check_wifi_connection_status(void)
{
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta_netif)
    {
        ESP_LOGE(TAG, "STA netif not found or not initialized");
        return ESP_ERR_INVALID_ARG;
    }

    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_STATE) {
             ESP_LOGW(TAG, "Wi-Fi STA is not connected to an AP (or interface not ready)");
        } else {
             ESP_LOGE(TAG, "Failed to get AP info: %s", esp_err_to_name(err));
        }
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(sta_netif, &ip_info) != ESP_OK || ip_info.ip.addr == 0)
    {
        ESP_LOGW(TAG, "No valid IP address assigned to STA netif");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Wi-Fi STA is connected to AP '%s' and has IP address: " IPSTR,
             ap_info.ssid, IP2STR(&ip_info.ip));
    return ESP_OK;
}
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "captive_portal.h"
#include "wifi.h"
#include "dwnvs.h"
#include "ledline/ledline.h"
#include "ledline/mqtt_ledline.h"

#define BOOT_RESET_THRESHOLD_MS   2000  // Время ожидания (мс)
#define BOOT_RESET_COUNT_TRIGGER  2     // Количество включений

static const char *TAG = "BootCheck";

//=================================================================
static void sta_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        char hostname[32] = {0};
        size_t hostname_len = sizeof(hostname);
        if (nvs_load_data("device", "hostname", hostname, &hostname_len, NVS_TYPE_STR) == ESP_OK)
        {
            dw_set_hostname_to_netif(WIFI_IF_STA, hostname);
        }
        mqtt_ledline_resources_init();
        dw_set_user_sta_event_handler(NULL);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
    }
}

//================================================================
static esp_err_t sta_connect_attempt(void)
{
    // Проверяем, подключено ли устройство к Wi-Fi сети (AP + IP)
    if (dw_check_wifi_connection_status() != ESP_OK)
    {
        wifi_sta_config_t sta_config = {0};
        if (dwnvs_load_sta_config(&sta_config) == ESP_OK)
        {
            dw_resources_init();
            dw_station_stop();
            if (dw_station_connect(&sta_config, sta_event_handler, NULL) == ESP_OK)
            {
                return ESP_OK; // Успешно начали процесс подключения
            }
            else
            {
                return ESP_FAIL; // Ошибка инициации подключения
            }
        }
        else
        {
            return ESP_FAIL; // Ошибка загрузки конфигурации
        }
    }
    else
    {
        mqtt_ledline_resources_init();
        return ESP_OK; // Уже подключены
    }
}

//================================================================
static bool check_factory_reset_mode()
{
    int reset_count = 0;

    size_t len = sizeof(reset_count);
    if (nvs_load_data("boot", "reset_count", &reset_count, &len, NVS_TYPE_I32) != ESP_OK) {
        reset_count = 0;
    }

    if (reset_count >= BOOT_RESET_COUNT_TRIGGER) {
        ESP_LOGI(TAG, "Factory reset mode triggered!");
        reset_count = 0;
        nvs_save_data("boot", "reset_count", &reset_count, sizeof(reset_count), NVS_TYPE_I32);
        return true;
    }

    reset_count++;

    nvs_save_data("boot", "reset_count", &reset_count, sizeof(reset_count), NVS_TYPE_I32);

    vTaskDelay(pdMS_TO_TICKS(BOOT_RESET_THRESHOLD_MS));

    reset_count = 0;
    nvs_save_data("boot", "reset_count", &reset_count, sizeof(reset_count), NVS_TYPE_I32);

    return false;
}

//================================================================
void app_main(void)
{
    nvs_storage_initialization();

    bool forced_launch = check_factory_reset_mode(); // <-- проверка

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ledline_resources_init();
    portal_start_with_sta_attempt("Ledline_config", "", forced_launch, sta_connect_attempt);

    while (1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
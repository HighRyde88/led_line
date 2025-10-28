#include "captive_portal.h"
#include "nvs_settings.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"

#include "wifi.h"
#include "dwnvs.h"

#include "ledline/ledline.h"
#include "ledline/mqtt_ledline.h"
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
        return ESP_OK; // Уже подключены
    }
}

//================================================================
void app_main(void)
{
    nvs_storage_initialization();

    gpio_reset_pin(GPIO_NUM_36);
    gpio_set_direction(GPIO_NUM_36, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_39, GPIO_PULLUP_ONLY);

    bool forced_launch = !gpio_get_level(GPIO_NUM_36);

    portal_start_with_sta_attempt("Ledline_config", "", forced_launch, sta_connect_attempt);

    ledline_resources_init();

    while (1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
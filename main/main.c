#include "captive_portal.h"
#include "nvs_settings.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"

#include "mqtt.h"
#include "wifi.h"

#include "ledline/ledline.h"
//================================================================
static void heap_monitor_task(void *pvParameter)
{
    while (1)
    {
        printf("\n=== Heap Information ===\n");
        printf("Free heap: %lu bytes\n", esp_get_free_heap_size());
        printf("Minimum free heap: %lu bytes\n", esp_get_minimum_free_heap_size());
        printf("Largest free block: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
        printf("========================\n\n");

        vTaskDelay(5000 / portTICK_PERIOD_MS); // Ждем 5 секунд
    }
}
//================================================================
void app_main(void)
{
    nvs_storage_initialization();

    // gpio_reset_pin(GPIO_NUM_36);
    // gpio_set_direction(GPIO_NUM_36, GPIO_MODE_INPUT);
    // gpio_set_pull_mode(GPIO_NUM_39, GPIO_PULLUP_ONLY);

    // xTaskCreate(&heap_monitor_task, "heap_monitor", 2048, NULL, 5, NULL);

    // captive_portal_start("", "", !gpio_get_level(GPIO_NUM_36));

    bool need_captive_portal = true;
    captive_portal_start("", "", need_captive_portal);
    ledline_resources_init();

    while (1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
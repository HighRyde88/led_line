#include "ledline.h"

led_strip_handle_t led_strip = NULL;
uint32_t leds_num = 0;

static const char *TAG = "led_strip";
//=================================================================
static led_strip_handle_t ledline_create(uint32_t lednum)
{
    ESP_LOGI(TAG, "Creating LED strip with %d LEDs", lednum);

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_PIN,
        .max_leds = lednum,
        .led_model = LED_MODEL_WS2812,

        .color_component_format = {
            .format = {
                .r_pos = 1,
                .g_pos = 0,
                .b_pos = 2,
                .num_components = 3,
            },
        },
        .flags = {
            .invert_out = false,
        }};

    led_strip_spi_config_t spi_config = {
        .clk_src = SPI_CLK_SRC_DEFAULT,
        .spi_bus = SPI2_HOST,
        .flags = {
            .with_dma = true,
        }};

    led_strip_handle_t handle = NULL;
    esp_err_t ret = led_strip_new_spi_device(&strip_config, &spi_config, &handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create LED strip object with SPI backend: %s", esp_err_to_name(ret));
        return NULL;
    }
    ESP_LOGI(TAG, "Successfully created LED strip object with SPI backend for %d LEDs", lednum);
    return handle;
}
//=================================================================
esp_err_t ledline_resources_deinit(led_strip_handle_t handle)
{
    if (handle != NULL)
    {
        ESP_LOGI(TAG, "Deleting LED strip object...");
        esp_err_t ret = led_strip_del(handle);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to delete LED strip object: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "LED strip object successfully deleted");
    }
    else
    {
        ESP_LOGW(TAG, "Attempted to delete NULL LED strip handle");
    }
    return ESP_OK;
}
//=================================================================
esp_err_t ledline_resources_init(void)
{
    ESP_LOGI(TAG, "Initializing LED strip resources...");

    char lednum_str[16] = {0};
    size_t str_size = sizeof(lednum_str);
    esp_err_t result = nvs_load_data("ledstrip", "lednum", lednum_str, &str_size, NVS_TYPE_STR);

    uint32_t lednum = 60;
    if (result == ESP_OK && str_size > 0 && lednum_str[0] != '\0')
    {
        lednum = atoi(lednum_str);
        ESP_LOGI(TAG, "Loaded LED count from NVS: %d", lednum);
    }
    else
    {
        ESP_LOGI(TAG, "Using default LED count: %d", lednum);
    }

    leds_num = lednum;
    led_strip = ledline_create(leds_num);
    if (led_strip == NULL)
    {
        ESP_LOGE(TAG, "Failed to create LED strip with %d LEDs", lednum);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LED strip resources initialized successfully with %d LEDs", lednum);
    return ESP_OK;
}
//=================================================================
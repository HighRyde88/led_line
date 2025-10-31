#include "ledline.h"
#include "effects_ledline.h"
#include "server/modules/data_parser.h"
#include "nvs_settings.h"

#define ITEMS_COUNT (3)

static const char *TAG = "Led effects";
//=================================================================
typedef bool (*topic_manager_func_t)(void *data);

typedef struct
{
    const char *topic;
    const topic_manager_func_t manager_func;
} topic_manager_t;

static bool state_manager(void *data);
static bool color_manager(void *data);
static bool brightness_manager(void *data);

static topic_manager_t topic_manager[] = {
    {"state", state_manager},
    {"color", color_manager},
    {"brightness", brightness_manager}};
static uint8_t topic_manager_count = sizeof(topic_manager) / sizeof(topic_manager_t);
//=================================================================
typedef void (*effect_manager_func_t)(void *data);

typedef struct
{
    const char *effect;
    const bool circular;
    const effect_manager_func_t effect_func;
} effect_manager_t;

static void static_effect(void *data);

static effect_manager_t effect_manager[] = {
    {"static", true, static_effect}};
static uint8_t effect_manager_count = sizeof(effect_manager) / sizeof(effect_manager_t);

static effect_manager_t *store_effect = NULL;
static effect_manager_t *current_effect = NULL;

static rgb_t current_color = {0};
static uint8_t current_brightness = 255;
//=================================================================
static bool state_manager(void *data)
{
    if (data == NULL)
        return true;

    char *state = (char *)data;
    return false;
}

static bool color_manager(void *data)
{
    if (data == NULL)
        return true;
    current_effect = &effect_manager[0];
    char *color_str = (char *)data;
    uint32_t color_int = color_string_to_uint32(color_str);
    current_color = rgb_from_code(color_int);
    return true;
}

static bool brightness_manager(void *data)
{
    if (data == NULL)
        return true;

    char *brightness_str = (char *)data;
    uint8_t brightness_percent = atoi(brightness_str);
    current_brightness = (uint8_t)((brightness_percent * 255) / 100);
    return true;
}
//=================================================================
void task_effects(void *pvParameters)
{
    mqtt_data_t data_message = {0};

    effect_manager_t *last_effect = NULL;
    bool effect_executed_once = false;

    while (1)
    {
        if (xQueueReceive(mqttQueue, &data_message, 100 / portTICK_PERIOD_MS) == pdTRUE)
        {
            if (topic_list != NULL && topic_count > 0)
            {
                bool func_executed = false;
                for (uint8_t i = 0; i < topic_count && !func_executed; i++)
                {
                    char *items[ITEMS_COUNT] = {0};
                    uint8_t count = split_string(data_message.topic, items, ITEMS_COUNT, '/');

                    if (count > 0 && items[count - 1] != NULL)
                    {
                        for (uint8_t j = 0; j < topic_manager_count && !func_executed; j++)
                        {
                            if (topic_manager[j].topic != NULL &&
                                strcmp(items[count - 1], topic_manager[j].topic) == 0)
                            {
                                if (topic_manager[j].manager_func(data_message.data) && data_message.data != NULL)
                                {
                                    free(data_message.data);
                                    data_message.data = NULL;
                                }

                                func_executed = true;

                                if (current_effect != last_effect)
                                {
                                    last_effect = current_effect;
                                }

                                if (current_effect != NULL && !current_effect->circular)
                                {
                                    effect_executed_once = false;
                                }
                            }
                        }
                    }

                    for (int k = 0; k < ITEMS_COUNT; k++)
                    {
                        if (items[k] != NULL)
                        {
                            free(items[k]);
                        }
                    }
                }
            }
        }

        if (current_effect != NULL && current_effect->effect_func != NULL)
        {
            if (current_effect->circular)
            {
                current_effect->effect_func(data_message.data);
                // Обнуляем data после выполнения
                if (data_message.data != NULL)
                {
                    free(data_message.data);  // освобождаем память
                    data_message.data = NULL; // обнуляем указатель
                }
            }
            else
            {
                if (!effect_executed_once)
                {
                    current_effect->effect_func(data_message.data);
                    effect_executed_once = true;
                    // Обнуляем data после выполнения
                    if (data_message.data != NULL)
                    {
                        free(data_message.data);  // освобождаем память
                        data_message.data = NULL; // обнуляем указатель
                    }
                }
                else
                {
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
            }
        }
    }
}
//=================================================================
void start_effects_ledline(void)
{
    uint32_t read_color = 0;
    size_t required_size = 0;
    esp_err_t result = nvs_load_data("ledline", "color", &read_color, &required_size, NVS_TYPE_U32);

    if (result == ESP_OK)
    {
        current_color = rgb_from_code(read_color);
        ESP_LOGI(TAG, "Color loaded from NVS: 0x%06X", read_color);
    }
    else if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        current_color = RGB_COLOR_DEFAULT();
        ESP_LOGW(TAG, "Color not found in NVS, using default color.");
    }
    else
    {
        current_color = RGB_COLOR_DEFAULT();
        ESP_LOGE(TAG, "Failed to load color from NVS: %s", esp_err_to_name(result));
    }
}
//=================================================================
static void change_static_color(rgb_t color)
{
    for (int i = 0; i <= 5; ++i)
    {
        fract16 frac = (fract16)((65535 * i) / 5);
        rgb_t result = rgb_lerp16(1, color, frac);

        for (uint32_t led = 0; led < leds_num; led++)
        {
            led_strip_set_pixel(led_strip, led, result.red, result.green, result.blue);
        }
        led_strip_refresh(led_strip);

        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}
static void static_effect(void *data)
{

}
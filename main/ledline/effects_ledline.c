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
static bool mode_manager(void *data);

static topic_manager_t topic_manager[] = {
    {"state", state_manager},
    {"color", color_manager},
    {"brightness", brightness_manager},
    {"mode", mode_manager}};
static uint8_t topic_manager_count = sizeof(topic_manager) / sizeof(topic_manager_t);
//=================================================================
typedef uint8_t (*effect_manager_func_t)(void);

typedef struct
{
    const char *effect;
    const bool circular;
    const effect_manager_func_t effect_func;
    const effect_manager_func_t effect_init;
} effect_manager_t;

static uint8_t static_effect_init(void);
static uint8_t static_effect(void);

static uint8_t gradient_effect_init(void);
static uint8_t gradient_effect(void);

static uint8_t rainbow_effect_init(void);
static uint8_t rainbow_effect(void);

static effect_manager_t effect_manager[] = {
    {"static", true, static_effect, static_effect_init},
    {"gradient", true, gradient_effect, gradient_effect_init},
    {"rainbow", true, rainbow_effect, rainbow_effect_init}};
static uint8_t effect_manager_count = sizeof(effect_manager) / sizeof(effect_manager_t);
//=================================================================
static hsv_t *led_buffer = NULL;
static bool is_need_update = false;

effect_manager_t *current_effect = NULL;
effect_manager_t *stored_effect = NULL;

bool current_state = false;

hsv_t filled_in_current_color = {0};
hsv_t filled_in_temp_color = {0};

uint8_t current_brightness = 0;
uint8_t stored_brightness = 0;

//=================================================================
static bool state_manager(void *data)
{
    if (data == NULL)
        return true;

    const char *state = (char *)data;
    mqtt_publish_state("state", state);

    if (strcmp(state, "enable") == 0)
    {
        if (current_effect == NULL && stored_effect != NULL)
        {
            current_effect = stored_effect;
        }
        else if (current_effect == NULL && stored_effect == NULL)
        {
            current_effect = &effect_manager[0];
        }

        stored_effect = NULL;

        current_brightness = stored_brightness;
        filled_in_current_color.val = current_brightness;
        current_state = true;
    }
    else if (strcmp(state, "disable") == 0)
    {
        current_brightness = 0;
        filled_in_current_color.val = 0;
        // current_state = false;
    }

    ESP_LOGI(TAG, "New state brightness: val - %d", current_brightness);

    return true;
}

//=================================================================
static bool color_manager(void *data)
{
    if (data == NULL)
        return true;

    mqtt_publish_state("mode", "static");

    const char *color_str = (char *)data;
    mqtt_publish_state("color", color_str);

    uint32_t color_int = color_from_hex(color_str);
    filled_in_current_color = color_to_hsv(color_int);
    filled_in_current_color.val = current_brightness;

    if (current_state == true)
    {
        current_effect = &effect_manager[0];
    }
    else
    {
        stored_effect = &effect_manager[0];
        memcpy(&filled_in_temp_color, &filled_in_current_color, sizeof(hsv_t));
    }

    ESP_LOGI(TAG, "New current color: HUE - %d, SAT - %d, VOL - %d",
             filled_in_current_color.hue, filled_in_current_color.sat, filled_in_current_color.val);

    nvs_save_data("ledline", "color", (void *)&color_int, sizeof(color_int), NVS_TYPE_U32);

    return true;
}

//=================================================================
static bool brightness_manager(void *data)
{
    if (data == NULL)
        return true;

    const char *brightness_str = (char *)data;
    mqtt_publish_state("brightness", brightness_str);

    uint8_t brightness_percent = atoi(brightness_str);
    current_brightness = (uint8_t)((brightness_percent * 255) / 100);
    filled_in_current_color.val = current_brightness;
    stored_brightness = current_brightness;

    ESP_LOGI(TAG, "New brightness: val - %d, percent - %d", current_brightness, brightness_percent);

    nvs_save_data("ledline", "brightness", (void *)&current_brightness, sizeof(current_brightness), NVS_TYPE_U8);

    return true;
}

//=================================================================
static bool mode_manager(void *data)
{
    if (data == NULL)
        return true;

    const char *mode_str = (char *)data;
    mqtt_publish_state("mode", mode_str);

    for (uint8_t m = 0; m < effect_manager_count; m++)
    {
        if (strcmp(mode_str, effect_manager[m].effect) == 0)
        {
            if (current_state == true)
            {
                current_effect = &effect_manager[m];
            }
            else
            {
                stored_effect = &effect_manager[m];
            }

            if (effect_manager[m].effect_init)
            {
                effect_manager[m].effect_init();
            }

            ESP_LOGI(TAG, "New current mode: %s", effect_manager[m].effect);
            break;
        }
    }

    return true;
}

//=================================================================
static void ledstrip_fill_buffer(void)
{
    for (uint16_t led = 0; led < leds_num; led++)
    {
        led_strip_set_pixel_hsv(led_strip, led, led_buffer[led].hue, led_buffer[led].sat, led_buffer[led].val);
    }
    led_strip_refresh(led_strip);
}

//=================================================================
static void fill_biffer_color(const hsv_t *color)
{
    for (uint16_t i = 0; i < leds_num; i++)
    {
        led_buffer[i].hue = color->hue;
        led_buffer[i].sat = color->sat;
        led_buffer[i].val = color->val;
    }
}

//=================================================================
static void task_effect_ledline(void *pvParameters)
{
    TickType_t xLastWakeTime = 0;
    while (1)
    {
        xLastWakeTime = xTaskGetTickCount();
        if (is_need_update)
        {
            ledstrip_fill_buffer();
            is_need_update = false;
        }
        vTaskDelayUntil(&xLastWakeTime, 30 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

//=================================================================
void task_mqtt_ledline(void *pvParameters)
{
    mqtt_data_t data_message = {0};
    TickType_t xLastWakeTime = 0;
    while (1)
    {
        xLastWakeTime = xTaskGetTickCount();
        if (xQueueReceive(mqttQueue, &data_message, 5 / portTICK_PERIOD_MS) == pdTRUE)
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

            // Освобождение topic, если оно было
            if (data_message.topic != NULL)
            {
                free(data_message.topic);
            }
        }

        if (current_effect != NULL && current_effect->effect_func != NULL)
        {
            uint8_t delay = current_effect->effect_func();
            if (delay > 0)
            {
                vTaskDelayUntil(&xLastWakeTime, delay / portTICK_PERIOD_MS);
                continue;
            }
        }
        vTaskDelayUntil(&xLastWakeTime, 25 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

//=================================================================
void start_effects_ledline(void)
{

    led_buffer = malloc(sizeof(hsv_t) * leds_num);

    if (led_buffer == NULL)
    {
        ESP_LOGI(TAG, "Buffer create failed, return...");
        return;
    }

    // === Загрузка цвета ===
    uint32_t read_color = 0;
    size_t color_size = sizeof(read_color);
    esp_err_t color_result = nvs_load_data("ledline", "color", &read_color, &color_size, NVS_TYPE_U32);

    if (color_result == ESP_OK)
    {
        filled_in_current_color = color_to_hsv(read_color);
        ESP_LOGI(TAG, "Color loaded from NVS: 0x%06X", read_color);
    }
    else if (color_result == ESP_ERR_NVS_NOT_FOUND)
    {
        filled_in_current_color = color_to_hsv(0x00A849B3); // RGB_COLOR_DEFAULT()
        ESP_LOGW(TAG, "Color not found in NVS, using default color.");
    }
    else
    {
        filled_in_current_color = color_to_hsv(0x00A849B3);
        ESP_LOGE(TAG, "Failed to load color from NVS: %s", esp_err_to_name(color_result));
    }

    // === Загрузка яркости ===
    uint8_t read_brightness = 0;
    size_t brightness_size = sizeof(read_brightness);
    esp_err_t brightness_result = nvs_load_data("ledline", "brightness", &read_brightness, &brightness_size, NVS_TYPE_U8);

    if (brightness_result == ESP_OK)
    {
        stored_brightness = read_brightness;
        ESP_LOGI(TAG, "Brightness loaded from NVS: %d", stored_brightness);
    }
    else if (brightness_result == ESP_ERR_NVS_NOT_FOUND)
    {
        // Используем яркость из загруженного/дефолтного цвета
        stored_brightness = 255;
        ESP_LOGW(TAG, "Brightness not found in NVS, using color's value: %d", stored_brightness);
    }
    else
    {
        stored_brightness = 255;
        ESP_LOGE(TAG, "Failed to load brightness from NVS: %s", esp_err_to_name(brightness_result));
    }

    ESP_LOGI(TAG, "Initial state: HUE=%d, SAT=%d, VAL=%d",
             filled_in_current_color.hue, filled_in_current_color.sat, filled_in_current_color.val);

    stored_effect = &effect_manager[0];

    led_strip_clear(led_strip);

    memcpy(&filled_in_temp_color, &filled_in_current_color, sizeof(hsv_t));
    filled_in_temp_color.val = 0;

    xTaskCreate(task_effect_ledline, "task_effect_ledline", 4096, NULL, 4, NULL);
}

//=================================================================
static uint8_t static_effect_init(void)
{
    uint32_t color_int = color_from_hsv(filled_in_current_color);
    char color_str[16] = {0};
    snprintf(color_str, sizeof(color_str), "#%06lX", color_int & 0x00FFFFFF);
    mqtt_publish_state("color", color_str);

    TickType_t xLastWakeTime = 0;
    while (1)
    {
        bool need_loop = false;
        xLastWakeTime = xTaskGetTickCount();
        for (uint16_t leds = 0; leds < leds_num; leds++)
        {
            if (color_hsv_interpolate(&led_buffer[leds], &filled_in_current_color, 30))
            {
                need_loop = true;
            }
        }

        is_need_update = true;

        if (!need_loop)
        {
            break;
        }
        vTaskDelayUntil(&xLastWakeTime, 30 / portTICK_PERIOD_MS);
    }

    return 0;
}

static uint8_t static_effect(void)
{
    while (color_hsv_interpolate(&filled_in_temp_color, &filled_in_current_color, 30))
    {
        fill_biffer_color(&filled_in_temp_color);
        is_need_update = true;
        return 0;
    }

    if (current_state == false && stored_effect == NULL)
    {
        stored_effect = current_effect;
        current_effect = NULL;
    }

    return 0;
}

//=================================================================
static uint8_t gradient_effect_init(void)
{
    TickType_t xLastWakeTime = 0;
    while (1)
    {
        bool need_loop = false;
        xLastWakeTime = xTaskGetTickCount();
        for (uint16_t leds = 0; leds < leds_num; leds++)
        {
            if (color_hsv_interpolate(&led_buffer[leds], &filled_in_current_color, 30))
            {
                need_loop = true;
            }
        }

        is_need_update = true;

        if (!need_loop)
        {
            break;
        }
        vTaskDelayUntil(&xLastWakeTime, 30 / portTICK_PERIOD_MS);
    }
    return 0;
}

static uint8_t gradient_effect(void)
{
    filled_in_current_color.hue = (filled_in_current_color.hue + 1) % 360;

    if (color_hsv_interpolate(&filled_in_temp_color, &filled_in_current_color, 30))
    {
        fill_biffer_color(&filled_in_temp_color);
        is_need_update = true;
    }
    else
    {
        if (current_state == false && stored_effect == NULL)
        {
            stored_effect = current_effect;
            current_effect = NULL;
        }
    }

    return 0;
}

//=================================================================
static uint8_t rainbow_effect_init(void)
{
    return 0;
}

static uint8_t rainbow_effect(void)
{
    static hsv_t temp_color = {0};

    temp_color.sat = filled_in_current_color.sat;
    temp_color.val = filled_in_current_color.val;

    bool need_loop = false;
    static uint16_t hue_start = 0;

    for (uint16_t led = 0; led < leds_num; led++)
    {
        temp_color.hue = (hue_start + led) % 360;

        if (color_hsv_interpolate(&led_buffer[led], &temp_color, 30))
        {
            need_loop = true;
        }

        is_need_update = true;

        if (!need_loop)
        {
            break;
        }
    }

    hue_start = (hue_start + 1) % 360;

    if (current_state == false && stored_effect == NULL)
    {
        stored_effect = current_effect;
        current_effect = NULL;
    }
    return 0;
}
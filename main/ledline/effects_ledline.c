#include "ledline.h"
#include "effects_ledline.h"
#include "server/modules/data_parser.h"

#define ITEMS_COUNT (3)

static const char *TAG = "Led effects";
//=================================================================
typedef void (*topic_manager_func_t)(void *data);

typedef struct
{
    const char *topic;
    const topic_manager_func_t manager_func;
} topic_manager_t;

static void state_manager(void *data);
static void color_manager(void *data);
static void brightness_manager(void *data);

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
static effect_manager_t *current_effect = &effect_manager[0];

static rgb_t current_color_rgb = {0};
//=================================================================
static void state_manager(void *data)
{
    if (data == NULL)
        return;

    char *state = (char *)data;

    if (strcmp(state, "enable") == 0)
    {
        current_effect = store_effect;
        store_effect = NULL;
    }
    else if (strcmp(state, "enable") == 0)
    {
        store_effect = current_effect;
        current_effect = NULL;
    }
}

static void color_manager(void *data)
{
    if (data == NULL)
        return;

    current_effect = &effect_manager[0];
}

static void brightness_manager(void *data)
{
    if (data == NULL)
        return;

    char *precent_str = (char *)data;
    uint8_t percent = atoi(precent_str);
    //current_brightness = (uint8_t)((percent * 255.0f) / 100.0f);
}
//=================================================================
void task_effects(void *pvParameters)
{
    mqtt_data_t data_message = {0};

    current_color_rgb.blue = 255;

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
                                topic_manager[j].manager_func(data_message.data);
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

        // Вызов эффекта
        if (current_effect != NULL && current_effect->effect_func != NULL)
        {
            if (current_effect->circular)
            {
                current_effect->effect_func(data_message.data);
            }
            else
            {
                if (!effect_executed_once)
                {
                    current_effect->effect_func(data_message.data);
                    effect_executed_once = true;
                }
            }
        }
    }
}
//=================================================================
static void static_effect(void *data)
{
    if (data == NULL)
    {
        return;
    }

    char *color_raw = (char *)data;
    uint32_t color_hex = color_string_to_uint32(color_raw);
    rgb_t target_color = rgb_from_code(color_hex);

    if(!compare_rgb(&target_color, &static_color_rgb)){

    }


    for (int i = 0; i <= 5; ++i)
    {
        fract16 frac = (fract16)((65535 * i) / 5);
        rgb_t result = rgb_lerp16(current_color_rgb, target_color, frac);

        for (uint32_t led = 0; led < leds_num; led++)
        {
            led_strip_set_pixel(led_strip, led, result.red, result.green, result.blue);
        }
        led_strip_refresh(led_strip);

        vTaskDelay(5 / portTICK_PERIOD_MS);
    }

    current_color_rgb = rgb_from_code(color_hex);
}
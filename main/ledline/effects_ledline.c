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
}

static void color_manager(void *data)
{
    if (data == NULL)
        return;
}

static void brightness_manager(void *data)
{
    if (data == NULL)
        return;
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
}
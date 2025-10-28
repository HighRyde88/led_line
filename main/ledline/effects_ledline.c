#include "effects_ledline.h"


#define ITEMS_COUNT (3)

static const char *TAG = "Led effects";

static uint8_t split_string(const char *input, char **output_array, int array_size, char delimiter);

typedef void (*topic_manager_func_t)(void *data);

typedef struct
{
    char *topic;
    topic_manager_func_t manager_func;
} topic_manager_t;

static void state_manager(void *data);
static topic_manager_t topic_manager[] = {
    {"state", state_manager}};
static uint8_t topic_manager_count = sizeof(topic_manager) / sizeof(topic_manager_t);
//=================================================================
static void state_manager(void *data)
{
    bool temp_state = (bool*)data;
}
//=================================================================
void task_effects(void *pvParameters)
{
    mqtt_data_t data_message = {0};

    while (1)
    {
        if (xQueueReceive(mqttQueue, &data_message, 100 / portTICK_PERIOD_MS) == pdTRUE)
        {
            if (topic_list != NULL && topic_count > 0)
            {
                for (uint8_t i = 0; i < topic_count; i++)
                {
                    char *items[ITEMS_COUNT] = {0};
                    uint8_t count = split_string(topic_list[i], items, ITEMS_COUNT, '/');

                    if (count > 0 && items[count - 1] != NULL)
                    {
                        for (uint8_t j = 0; j < topic_manager_count; j++)
                        {
                            if (strcmp(items[count - 1], topic_manager[j].topic) == 0)
                            {
                                topic_manager[j].manager_func(data_message.data);
                                break;
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
    }
}
//=================================================================
static uint8_t split_string(const char *input, char **output_array, int array_size, char delimiter)
{
    if (!input || !output_array || array_size <= 0)
    {
        return 0;
    }

    int count = 0;
    const char *start = input;
    const char *end = input;

    while (*end && count < array_size)
    {
        if (*end == delimiter)
        {
            int len = end - start;
            output_array[count] = malloc(len + 1);
            if (output_array[count] != NULL)
            {
                strncpy(output_array[count], start, len);
                output_array[count][len] = '\0';
                count++;
                start = end + 1;
            }
        }
        end++;
    }

    if (count < array_size && start < end)
    {
        int len = end - start;
        output_array[count] = malloc(len + 1);
        if (output_array[count] != NULL)
        {
            strncpy(output_array[count], start, len);
            output_array[count][len] = '\0';
            count++;
        }
    }

    return count;
}
#ifndef __EFFECTS_H
#define __EFFECTS_H

#include "color.h"
#include "led_strip.h"
#include "mqtt_ledline.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void task_effects(void *pvParameters);

    // Support functions
    uint32_t color_string_to_uint32(const char* color_str);
    uint8_t split_string(const char *input, char **output_array, int array_size, char delimiter);
    int compare_rgb(const rgb_t* a, const rgb_t* b);

#ifdef __cplusplus
}
#endif

#endif
#ifndef __EFFECTS_H
#define __EFFECTS_H

#include "led_strip.h"
#include "mqtt_ledline.h"

#define RGB_COLOR_DEFAULT() ((rgb_t){.r = 168, .g = 73, .b = 179})

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        uint16_t hue;
        uint8_t sat;
        uint8_t val;
    } hsv_t;

    typedef struct
    {
        hsv_t start;
        hsv_t target;
        uint16_t total_steps;
        uint16_t step;
        bool active;
    } hsv_interpolation_state_t;

    extern bool current_state;
    extern hsv_t current_color;
    extern uint8_t stored_brightness;

    void start_effects_ledline(void);
    void task_mqtt_ledline(void *pvParameters);

    // Support functions
    uint32_t color_from_hex(const char *hex_str);
    uint8_t split_string(const char *input, char **output_array, int array_size, char delimiter);
    hsv_t color_to_hsv(uint32_t color);
    uint32_t color_from_hsv(hsv_t hsv);
    bool color_hsv_equal(const hsv_t *a, const hsv_t *b);
    void hsv_interpolate_step(hsv_t *current, const hsv_t *target, uint16_t total_steps, hsv_interpolation_state_t *state);

#ifdef __cplusplus
}
#endif

#endif
#ifndef __LEDLINE_H
#define __LEDLINE_H

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"
#include "mqtt.h"
#include "nvs_settings.h"
#include "driver/gpio.h"

#define LED_STRIP_GPIO_PIN GPIO_NUM_48

#ifdef __cplusplus
extern "C"
{
#endif

    extern uint32_t leds_num;
    extern led_strip_handle_t led_strip;
    
    esp_err_t ledline_resources_init(void);
    esp_err_t ledline_resources_deinit(led_strip_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif
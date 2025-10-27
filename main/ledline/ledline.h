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

#define LED_STRIP_GPIO_PIN 2

#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t ledline_resources_init(void);
    esp_err_t ledline_resources_deinit(led_strip_handle_t handle);

    esp_err_t mqtt_ledline_resources_init(void);
    void mqtt_ledline_resources_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
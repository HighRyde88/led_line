#ifndef __MQTTLEDLINE_H
#define __MQTTLEDLINE_H

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"
#include "mqtt.h"
#include "nvs_settings.h"
#include "driver/gpio.h"
#include "ledline.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        char *topic;
        void *data;
    } mqtt_data_t;

    extern int topic_count;
    extern char **topic_list;
    
    esp_err_t mqtt_ledline_resources_init(void);
    esp_err_t mqtt_publish_state(const char *topic_suffix, const char *payload);
    void mqtt_ledline_resources_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
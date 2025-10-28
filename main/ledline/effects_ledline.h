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

#ifdef __cplusplus
}
#endif

#endif
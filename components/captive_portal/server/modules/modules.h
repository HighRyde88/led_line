#ifndef __SYSHAND_H
#define __SYSAND_H

#include "cJSON.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t device_module_target(cJSON *data);
    esp_err_t control_module_target(cJSON *json);
    esp_err_t ledstrip_module_target(cJSON *json);
    esp_err_t wifi_module_target(cJSON *json);
    esp_err_t network_module_target(cJSON *json);
    esp_err_t apoint_module_target(cJSON *json);
    esp_err_t mqtt_module_target(cJSON *json);
    esp_err_t update_module_target(cJSON *json);

    void send_response_json(const char *type, const char *target, const char *status, const void *message, bool isObject);

#ifdef __cplusplus
}
#endif

#endif
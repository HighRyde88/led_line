#ifndef __DATAPARSE_H
#define __DATAPARSE_H

#include "cJSON.h"
#include "esp_err.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        const char *json_key;
        const char *nvs_key;
    } config_param_t;

    // Загрузка и сохранение обычных строковых настроек
    cJSON *load_and_parse_json_settings(const char *namespace, const config_param_t *config_params, size_t params_count);
    esp_err_t parse_and_save_json_settings(const char *namespace, cJSON *json, const config_param_t *config_params, size_t params_count);

    // Загрузка и сохранение IP-настроек (static/dhcp)
    cJSON *load_and_parse_json_ip_info(const char *namespace);
    esp_err_t parse_and_save_json_ip_info(const char *namespace, cJSON *json, esp_netif_ip_info_t *ipinfo);

#ifdef __cplusplus
}
#endif

#endif // __DATAPARSE_H
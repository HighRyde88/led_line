#include "esp_err.h"
#include "cJSON.h"
#include "nvs_settings.h"
#include "string.h"
#include "server/server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "captive_portal.h"
#include "esp_mac.h"
#include "esp_chip_info.h"
#include "esp_netif.h"
#include "data_parser.h"
#include "dwnvs.h"

static const char *TAG = "DATAPARSER";

#define MAX_CONFIG_VALUE_LENGTH 255
//=================================================================
esp_err_t parse_and_save_json_settings(const char *namespace, cJSON *json, const config_param_t *config_params, size_t params_count)
{
    if (!cJSON_IsObject(json) || config_params == NULL || params_count == 0)
        return ESP_ERR_INVALID_ARG;

    esp_err_t overall_err = ESP_OK;
    bool found_any = false;

    for (size_t i = 0; i < params_count; i++)
    {
        cJSON *item = cJSON_GetObjectItemCaseSensitive(json, config_params[i].json_key);
        if (item == NULL)
        {
            ESP_LOGW(TAG, "Key '%s' not found in JSON", config_params[i].json_key);
            continue; // Пропускаем, если ключ не найден
        }

        if (!cJSON_IsString(item) || item->valuestring == NULL)
        {
            ESP_LOGE(TAG, "Key '%s' is not a valid string", config_params[i].json_key);
            continue; // Пропускаем, если не строка
        }

        size_t len = strlen(item->valuestring);
        if (len == 0)
        {
            // Удаляем запись, если строка пустая
            esp_err_t err = nvs_delete_data(namespace, config_params[i].nvs_key);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to erase key '%s': %s", config_params[i].nvs_key, esp_err_to_name(err));
                if (overall_err == ESP_OK)
                {
                    overall_err = err;
                }
            }
            else
            {
                ESP_LOGI(TAG, "Erased key '%s' due to empty value", config_params[i].nvs_key);
            }
            found_any = true;
            continue;
        }

        if (len > MAX_CONFIG_VALUE_LENGTH)
        {
            ESP_LOGE(TAG, "Value for key '%s' invalid length: %zu", config_params[i].json_key, len);
            continue;
        }

        found_any = true;

        esp_err_t err = nvs_save_data(namespace,
                                      config_params[i].nvs_key,
                                      item->valuestring,
                                      len + 1,
                                      NVS_TYPE_STR);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to save key '%s': %s", config_params[i].nvs_key, esp_err_to_name(err));

            if (overall_err == ESP_OK)
            {
                overall_err = err; // Сохраняем первую ошибку
            }
        }
    }

    if (!found_any)
    {
        ESP_LOGW(TAG, "No valid keys found in JSON for saving");
        return ESP_FAIL; // или ESP_ERR_NOT_FOUND, если хотите
    }

    return overall_err;
}
//=================================================================
cJSON *load_and_parse_json_settings(const char *namespace, const config_param_t *config_params, size_t params_count)
{
    if (config_params == NULL || params_count == 0 || namespace == NULL)
        return NULL;

    cJSON *json = cJSON_CreateObject();
    if (json == NULL)
        return NULL;

    for (size_t i = 0; i < params_count; i++)
    {
        size_t required_size = 0;
        esp_err_t err = nvs_load_data(namespace,
                                      config_params[i].nvs_key,
                                      NULL,
                                      &required_size,
                                      NVS_TYPE_STR);

        if (err == ESP_OK && required_size > 0)
        {
            char *value = malloc(required_size);
            if (value != NULL)
            {
                err = nvs_load_data(namespace,
                                    config_params[i].nvs_key,
                                    value,
                                    &required_size,
                                    NVS_TYPE_STR);

                if (err == ESP_OK)
                {
                    cJSON_AddStringToObject(json, config_params[i].json_key, value);
                }
                else
                {
                    cJSON_AddNullToObject(json, config_params[i].json_key);
                }
                free(value);
            }
            else
            {
                cJSON_AddNullToObject(json, config_params[i].json_key);
                ESP_LOGE(TAG, "Memory alloc failed for key: %s", config_params[i].json_key);
            }
        }
        else if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            cJSON_AddNullToObject(json, config_params[i].json_key);
        }
        else if (err != ESP_OK)
        {
            cJSON_AddNullToObject(json, config_params[i].json_key);
        }
    }

    return json;
}
//=================================================================
esp_err_t parse_and_save_json_ip_info(const char *namespace, cJSON *json, esp_netif_ip_info_t *ipinfo)
{
    if (!cJSON_IsObject(json) || namespace == NULL)
        return ESP_ERR_INVALID_ARG;

    cJSON *mode_item = cJSON_GetObjectItemCaseSensitive(json, "mode");
    if (!cJSON_IsString(mode_item) || mode_item->valuestring == NULL)
    {
        ESP_LOGE(TAG, "Mode field is missing or invalid");
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(mode_item->valuestring, "dhcp") == 0)
    {
        esp_err_t err = dwnvs_delete_ipinfo(namespace);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to delete IP info: %s", esp_err_to_name(err));
            return err;
        }

        if (ipinfo != NULL)
        {
            memset(ipinfo, 0, sizeof(esp_netif_ip_info_t));
        }
        return ESP_OK;
    }
    else if (strcmp(mode_item->valuestring, "static") == 0)
    {
        esp_netif_ip_info_t local_ip_info = {0};
        bool has_ip = false, has_gw = false, has_nm = false;

        cJSON *ip_item = cJSON_GetObjectItemCaseSensitive(json, "ip");
        if (cJSON_IsString(ip_item) && ip_item->valuestring != NULL)
        {
            if (esp_netif_str_to_ip4(ip_item->valuestring, &local_ip_info.ip) == ESP_OK)
            {
                has_ip = true;
            }
            else
            {
                ESP_LOGE(TAG, "Invalid IP: %s", ip_item->valuestring);
                return ESP_ERR_INVALID_ARG;
            }
        }

        cJSON *gw_item = cJSON_GetObjectItemCaseSensitive(json, "gateway");
        if (cJSON_IsString(gw_item) && gw_item->valuestring != NULL)
        {
            if (esp_netif_str_to_ip4(gw_item->valuestring, &local_ip_info.gw) == ESP_OK)
            {
                has_gw = true;
            }
            else
            {
                ESP_LOGE(TAG, "Invalid GW: %s", gw_item->valuestring);
                return ESP_ERR_INVALID_ARG;
            }
        }

        cJSON *nm_item = cJSON_GetObjectItemCaseSensitive(json, "netmask");
        if (cJSON_IsString(nm_item) && nm_item->valuestring != NULL)
        {
            if (esp_netif_str_to_ip4(nm_item->valuestring, &local_ip_info.netmask) == ESP_OK)
            {
                has_nm = true;
            }
            else
            {
                ESP_LOGE(TAG, "Invalid NM: %s", nm_item->valuestring);
                return ESP_ERR_INVALID_ARG;
            }
        }

        if (!has_ip || !has_gw || !has_nm)
        {
            ESP_LOGE(TAG, "Static mode requires: ip, gw, nm");
            return ESP_ERR_INVALID_ARG;
        }

        esp_err_t err = dwnvs_save_ipinfo(namespace, &local_ip_info);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to save IP info: %s", esp_err_to_name(err));
            return err;
        }

        if (ipinfo != NULL)
        {
            memcpy(ipinfo, &local_ip_info, sizeof(esp_netif_ip_info_t));
        }
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Unknown mode: %s", mode_item->valuestring);
        return ESP_ERR_INVALID_ARG;
    }
}
//=================================================================
cJSON *load_and_parse_json_ip_info(const char *namespace)
{
    if (namespace == NULL)
        return NULL;

    cJSON *json = cJSON_CreateObject();
    if (json == NULL)
        return NULL;

    esp_netif_ip_info_t ip_info = {0};
    esp_err_t err = dwnvs_load_ipinfo(namespace, &ip_info);

    if (err == ESP_OK)
    {
        cJSON_AddStringToObject(json, "mode", "static");

        char ip_str[16] = {0}, gw_str[16] = {0}, nm_str[16] = {0};
        esp_ip4_addr_t ip4_addr;

        ip4_addr.addr = ip_info.ip.addr;
        esp_ip4addr_ntoa(&ip4_addr, ip_str, sizeof(ip_str));
        cJSON_AddStringToObject(json, "ip", ip_str);

        ip4_addr.addr = ip_info.gw.addr;
        esp_ip4addr_ntoa(&ip4_addr, gw_str, sizeof(gw_str));
        cJSON_AddStringToObject(json, "gateway", gw_str);

        ip4_addr.addr = ip_info.netmask.addr;
        esp_ip4addr_ntoa(&ip4_addr, nm_str, sizeof(nm_str));
        cJSON_AddStringToObject(json, "netmask", nm_str);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        cJSON_AddStringToObject(json, "mode", "dhcp");
        cJSON_AddNullToObject(json, "ip");
        cJSON_AddNullToObject(json, "gw");
        cJSON_AddNullToObject(json, "nm");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to load IP info: %s", esp_err_to_name(err));
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}
//=================================================================
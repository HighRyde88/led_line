#include "esp_err.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>
#include "freertos/semphr.h"

#define WIFISTA_NAMESPACE "DWNVS_STA"
#define WIFIAP_NAMESPACE "DWNVS_AP"
#define WIFICFG_NAMESPACE "DWNVS_CFG"

static const char *TAG = "DWNVS";
static SemaphoreHandle_t dwnvs_mutex = NULL;
//=================================================================
static bool dwnvs_mutex_lock(TickType_t timeout)
{
    if (dwnvs_mutex == NULL)
    {
        return false;
    }
    return xSemaphoreTake(dwnvs_mutex, timeout) == pdTRUE;
}

static void dwnvs_mutex_unlock(void)
{
    if (dwnvs_mutex != NULL)
    {
        xSemaphoreGive(dwnvs_mutex);
    }
}
//=================================================================
esp_err_t dwnvs_storage_initialization(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS partition has no free pages or new version detected. Erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "NVS initialized successfully");
        // Создаем мьютекс для синхронизации
        if (dwnvs_mutex == NULL)
        {
            dwnvs_mutex = xSemaphoreCreateMutex();
            if (dwnvs_mutex == NULL)
            {
                ESP_LOGE(TAG, "Failed to create NVS mutex");
                return ESP_FAIL;
            }
        }
    }
    return err;
}

esp_err_t dwnvs_save_sta_config(const wifi_sta_config_t *sta_config)
{
    if (sta_config == NULL)
    {
        ESP_LOGE(TAG, "STA config is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!dwnvs_mutex_lock(portMAX_DELAY))
    {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(WIFISTA_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        dwnvs_mutex_unlock();
        return err;
    }

    err = nvs_set_blob(nvs_handle, "sta_config", sta_config, sizeof(wifi_sta_config_t));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save STA config blob: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        dwnvs_mutex_unlock();
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    dwnvs_mutex_unlock();
    return err;
}

esp_err_t dwnvs_load_sta_config(wifi_sta_config_t *sta_config)
{
    if (sta_config == NULL)
    {
        ESP_LOGE(TAG, "STA config pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!dwnvs_mutex_lock(portMAX_DELAY))
    {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t required_size = sizeof(wifi_sta_config_t);

    err = nvs_open(WIFISTA_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        dwnvs_mutex_unlock();
        return err;
    }

    memset(sta_config, 0, sizeof(wifi_sta_config_t));

    err = nvs_get_blob(nvs_handle, "sta_config", sta_config, &required_size);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read STA config blob: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        dwnvs_mutex_unlock();
        return (err == ESP_ERR_NVS_NOT_FOUND) ? ESP_ERR_NVS_NOT_FOUND : err;
    }

    if (required_size != sizeof(wifi_sta_config_t))
    {
        ESP_LOGE(TAG, "STA config size mismatch: expected %d, got %d",
                 sizeof(wifi_sta_config_t), required_size);
        nvs_close(nvs_handle);
        dwnvs_mutex_unlock();
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "STA config loaded successfully: SSID=%s", sta_config->ssid);
    nvs_close(nvs_handle);
    dwnvs_mutex_unlock();
    return ESP_OK;
}

esp_err_t dwnvs_delete_sta_config(void)
{
    if (!dwnvs_mutex_lock(portMAX_DELAY))
    {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(WIFISTA_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        dwnvs_mutex_unlock();
        return err;
    }

    err = nvs_erase_key(nvs_handle, "sta_config");
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "Failed to erase STA config: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        dwnvs_mutex_unlock();
        return err;
    }

    ESP_LOGI(TAG, "STA config deleted");

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    dwnvs_mutex_unlock();
    return err;
}
//=================================================================
esp_err_t dwnvs_save_ap_config(const wifi_ap_config_t *ap_config)
{
    if (ap_config == NULL)
    {
        ESP_LOGE(TAG, "AP config is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!dwnvs_mutex_lock(portMAX_DELAY))
    {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(WIFIAP_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        dwnvs_mutex_unlock();
        return err;
    }

    // Сохраняем конфигурацию AP как BLOB
    err = nvs_set_blob(nvs_handle, "ap_config", ap_config, sizeof(wifi_ap_config_t));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save AP config blob: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        dwnvs_mutex_unlock();
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    dwnvs_mutex_unlock();
    return err;
}

esp_err_t dwnvs_load_ap_config(wifi_ap_config_t *ap_config)
{
    if (ap_config == NULL)
    {
        ESP_LOGE(TAG, "AP config pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!dwnvs_mutex_lock(portMAX_DELAY))
    {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t required_size = sizeof(wifi_ap_config_t);

    err = nvs_open(WIFIAP_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        dwnvs_mutex_unlock();
        return err;
    }

    memset(ap_config, 0, sizeof(wifi_ap_config_t));

    err = nvs_get_blob(nvs_handle, "ap_config", ap_config, &required_size);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read AP config blob: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        dwnvs_mutex_unlock();
        return (err == ESP_ERR_NVS_NOT_FOUND) ? ESP_ERR_NVS_NOT_FOUND : err;
    }

    if (required_size != sizeof(wifi_ap_config_t))
    {
        ESP_LOGE(TAG, "AP config size mismatch: expected %d, got %d",
                 sizeof(wifi_ap_config_t), required_size);
        nvs_close(nvs_handle);
        dwnvs_mutex_unlock();
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "AP config loaded successfully: SSID=%s", ap_config->ssid);
    nvs_close(nvs_handle);
    dwnvs_mutex_unlock();
    return ESP_OK;
}

esp_err_t dwnvs_delete_ap_config(void)
{
    if (!dwnvs_mutex_lock(portMAX_DELAY))
    {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(WIFIAP_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        dwnvs_mutex_unlock();
        return err;
    }

    err = nvs_erase_key(nvs_handle, "ap_config");
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "Failed to erase AP config: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        dwnvs_mutex_unlock();
        return err;
    }

    ESP_LOGI(TAG, "AP config deleted");

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    dwnvs_mutex_unlock();
    return err;
}
//=================================================================
//=================================================================
esp_err_t dwnvs_save_ipinfo(const char *namespace, const esp_netif_ip_info_t *ipinfo)
{
    if (ipinfo == NULL)
    {
        ESP_LOGE(TAG, "IP info is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!dwnvs_mutex_lock(portMAX_DELAY))
    {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(namespace, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        dwnvs_mutex_unlock();
        return err;
    }

    // Сохраняем IP информацию как BLOB
    err = nvs_set_blob(nvs_handle, "ip_info", ipinfo, sizeof(esp_netif_ip_info_t));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save IP info blob: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        dwnvs_mutex_unlock();
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    dwnvs_mutex_unlock();
    return err;
}

esp_err_t dwnvs_load_ipinfo(const char *namespace, esp_netif_ip_info_t *ipinfo)
{
    if (ipinfo == NULL)
    {
        ESP_LOGE(TAG, "IP info pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!dwnvs_mutex_lock(portMAX_DELAY))
    {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t required_size = sizeof(esp_netif_ip_info_t);

    err = nvs_open(namespace, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        dwnvs_mutex_unlock();
        return err;
    }

    memset(ipinfo, 0, sizeof(esp_netif_ip_info_t));

    err = nvs_get_blob(nvs_handle, "ip_info", ipinfo, &required_size);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read IP info blob: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        dwnvs_mutex_unlock();
        return (err == ESP_ERR_NVS_NOT_FOUND) ? ESP_ERR_NVS_NOT_FOUND : err;
    }

    if (required_size != sizeof(esp_netif_ip_info_t))
    {
        ESP_LOGE(TAG, "IP info size mismatch: expected %d, got %d",
                 sizeof(esp_netif_ip_info_t), required_size);
        nvs_close(nvs_handle);
        dwnvs_mutex_unlock();
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "IP info loaded successfully");
    nvs_close(nvs_handle);
    dwnvs_mutex_unlock();
    return ESP_OK;
}

esp_err_t dwnvs_delete_ipinfo(const char *namespace)
{
    if (!dwnvs_mutex_lock(portMAX_DELAY))
    {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(namespace, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        dwnvs_mutex_unlock();
        return err;
    }

    err = nvs_erase_key(nvs_handle, "ip_info");
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "Failed to erase IP info: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        dwnvs_mutex_unlock();
        return err;
    }

    ESP_LOGI(TAG, "IP info deleted");

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    dwnvs_mutex_unlock();
    return err;
}
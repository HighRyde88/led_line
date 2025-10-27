#include "esp_err.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>
#include "freertos/semphr.h"

#define CFG_NAMESPACE "data_config"

static const char *TAG = "NVS";
static SemaphoreHandle_t nvs_mutex = NULL;

static bool nvs_mutex_lock(TickType_t timeout)
{
    if (nvs_mutex == NULL)
    {
        return false;
    }
    if (xSemaphoreTake(nvs_mutex, timeout) == pdTRUE)
    {
        ESP_LOGW(TAG, "NVS mutex locked");
        return true;
    }
    ESP_LOGE(TAG, "Failed to lock NVS mutex (timeout or error)");
    return false;
}

static void nvs_mutex_unlock(void)
{
    if (nvs_mutex != NULL)
    {
        xSemaphoreGive(nvs_mutex);
        ESP_LOGW(TAG, "NVS mutex unlocked");
    }
}

esp_err_t nvs_storage_initialization(void)
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
        // Создаем мьютекс для синхронизации
        if (nvs_mutex == NULL)
        {
            nvs_mutex = xSemaphoreCreateMutex();
            if (nvs_mutex == NULL)
            {
                ESP_LOGE(TAG, "Failed to create NVS mutex");
                return ESP_FAIL;
            }
        }
    }
    return err;
}

void nvs_storage_deinit(void)
{
    if (nvs_mutex)
    {
        vSemaphoreDelete(nvs_mutex);
        nvs_mutex = NULL;
    }
}

esp_err_t nvs_save_data(const char *namespace, const char *key, const void *data, size_t length, nvs_type_t type)
{
    if (namespace == NULL || key == NULL || data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!nvs_mutex_lock(portMAX_DELAY))
    {
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(namespace, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        nvs_mutex_unlock();
        return err;
    }

    switch (type)
    {
    case NVS_TYPE_U8:
        if (length != sizeof(uint8_t))
        {
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        err = nvs_set_u8(handle, key, *(uint8_t *)data);
        break;

    case NVS_TYPE_I8:
        if (length != sizeof(int8_t))
        {
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        err = nvs_set_i8(handle, key, *(int8_t *)data);
        break;

    case NVS_TYPE_U16:
        if (length != sizeof(uint16_t))
        {
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        err = nvs_set_u16(handle, key, *(uint16_t *)data);
        break;

    case NVS_TYPE_I16:
        if (length != sizeof(int16_t))
        {
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        err = nvs_set_i16(handle, key, *(int16_t *)data);
        break;

    case NVS_TYPE_U32:
        if (length != sizeof(uint32_t))
        {
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        err = nvs_set_u32(handle, key, *(uint32_t *)data);
        break;

    case NVS_TYPE_I32:
        if (length != sizeof(int32_t))
        {
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        err = nvs_set_i32(handle, key, *(int32_t *)data);
        break;

    case NVS_TYPE_U64:
        if (length != sizeof(uint64_t))
        {
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        err = nvs_set_u64(handle, key, *(uint64_t *)data);
        break;

    case NVS_TYPE_I64:
        if (length != sizeof(int64_t))
        {
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        err = nvs_set_i64(handle, key, *(int64_t *)data);
        break;

    case NVS_TYPE_STR:
        err = nvs_set_str(handle, key, (const char *)data);
        break;

    case NVS_TYPE_BLOB:
        if (length == 0)
        {
            err = ESP_ERR_INVALID_ARG;
            break;
        }
        err = nvs_set_blob(handle, key, data, length);
        break;

    default:
        err = ESP_ERR_INVALID_ARG;
        break;
    }

    if (err != ESP_OK)
    {
        nvs_close(handle);
        nvs_mutex_unlock();
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    nvs_mutex_unlock();
    return err;
}

esp_err_t nvs_load_data(const char *namespace, const char *key, void *out_data, size_t *length, nvs_type_t type)
{
    if (namespace == NULL || key == NULL || length == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!nvs_mutex_lock(portMAX_DELAY))
    {
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(namespace, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        nvs_mutex_unlock();
        return err;
    }

    switch (type)
    {
    case NVS_TYPE_U8:
        if (out_data != NULL && *length < sizeof(uint8_t))
        {
            *length = sizeof(uint8_t);
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        if (out_data != NULL)
        {
            err = nvs_get_u8(handle, key, (uint8_t *)out_data);
        }
        else
        {
            uint8_t temp;
            err = nvs_get_u8(handle, key, &temp);
        }
        if (err == ESP_OK)
            *length = sizeof(uint8_t);
        break;

    case NVS_TYPE_I8:
        if (out_data != NULL && *length < sizeof(int8_t))
        {
            *length = sizeof(int8_t);
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        if (out_data != NULL)
        {
            err = nvs_get_i8(handle, key, (int8_t *)out_data);
        }
        else
        {
            int8_t temp;
            err = nvs_get_i8(handle, key, &temp);
        }
        if (err == ESP_OK)
            *length = sizeof(int8_t);
        break;

    case NVS_TYPE_U16:
        if (out_data != NULL && *length < sizeof(uint16_t))
        {
            *length = sizeof(uint16_t);
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        if (out_data != NULL)
        {
            err = nvs_get_u16(handle, key, (uint16_t *)out_data);
        }
        else
        {
            uint16_t temp;
            err = nvs_get_u16(handle, key, &temp);
        }
        if (err == ESP_OK)
            *length = sizeof(uint16_t);
        break;

    case NVS_TYPE_I16:
        if (out_data != NULL && *length < sizeof(int16_t))
        {
            *length = sizeof(int16_t);
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        if (out_data != NULL)
        {
            err = nvs_get_i16(handle, key, (int16_t *)out_data);
        }
        else
        {
            int16_t temp;
            err = nvs_get_i16(handle, key, &temp);
        }
        if (err == ESP_OK)
            *length = sizeof(int16_t);
        break;

    case NVS_TYPE_U32:
        if (out_data != NULL && *length < sizeof(uint32_t))
        {
            *length = sizeof(uint32_t);
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        if (out_data != NULL)
        {
            err = nvs_get_u32(handle, key, (uint32_t *)out_data);
        }
        else
        {
            uint32_t temp;
            err = nvs_get_u32(handle, key, &temp);
        }
        if (err == ESP_OK)
            *length = sizeof(uint32_t);
        break;

    case NVS_TYPE_I32:
        if (out_data != NULL && *length < sizeof(int32_t))
        {
            *length = sizeof(int32_t);
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        if (out_data != NULL)
        {
            err = nvs_get_i32(handle, key, (int32_t *)out_data);
        }
        else
        {
            int32_t temp;
            err = nvs_get_i32(handle, key, &temp);
        }
        if (err == ESP_OK)
            *length = sizeof(int32_t);
        break;

    case NVS_TYPE_U64:
        if (out_data != NULL && *length < sizeof(uint64_t))
        {
            *length = sizeof(uint64_t);
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        if (out_data != NULL)
        {
            err = nvs_get_u64(handle, key, (uint64_t *)out_data);
        }
        else
        {
            uint64_t temp;
            err = nvs_get_u64(handle, key, &temp);
        }
        if (err == ESP_OK)
            *length = sizeof(uint64_t);
        break;

    case NVS_TYPE_I64:
        if (out_data != NULL && *length < sizeof(int64_t))
        {
            *length = sizeof(int64_t);
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        if (out_data != NULL)
        {
            err = nvs_get_i64(handle, key, (int64_t *)out_data);
        }
        else
        {
            int64_t temp;
            err = nvs_get_i64(handle, key, &temp);
        }
        if (err == ESP_OK)
            *length = sizeof(int64_t);
        break;

    case NVS_TYPE_STR:
    {
        size_t required_size = 0;
        err = nvs_get_str(handle, key, NULL, &required_size);
        if (err != ESP_OK)
            break;
        if (out_data != NULL && *length < required_size)
        {
            *length = required_size;
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        if (out_data != NULL)
        {
            err = nvs_get_str(handle, key, (char *)out_data, &required_size);
        }
        if (err == ESP_OK)
            *length = required_size;
        break;
    }

    case NVS_TYPE_BLOB:
    {
        size_t required_size = 0;
        err = nvs_get_blob(handle, key, NULL, &required_size);
        if (err != ESP_OK)
            break;
        if (out_data != NULL && *length < required_size)
        {
            *length = required_size;
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        if (out_data != NULL)
        {
            err = nvs_get_blob(handle, key, out_data, &required_size);
        }
        if (err == ESP_OK)
            *length = required_size;
        break;
    }

    default:
        err = ESP_ERR_INVALID_ARG;
        break;
    }

    nvs_close(handle);
    nvs_mutex_unlock();
    return err;
}

esp_err_t nvs_delete_data(const char *namespace, const char *key)
{
    if (namespace == NULL || key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!nvs_mutex_lock(portMAX_DELAY))
    {
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(namespace, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        nvs_mutex_unlock();
        return err;
    }

    err = nvs_erase_key(handle, key);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(handle);
        nvs_mutex_unlock();
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    nvs_mutex_unlock();
    return err;
}

esp_err_t nvs_clear_namespace(const char *namespace)
{
    if (namespace == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!nvs_mutex_lock(portMAX_DELAY))
    {
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(namespace, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        nvs_mutex_unlock();
        return err;
    }

    err = nvs_erase_all(handle);
    if (err != ESP_OK)
    {
        nvs_close(handle);
        nvs_mutex_unlock();
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    nvs_mutex_unlock();
    return err;
}

esp_err_t nvs_set_flag(const char *key, bool value)
{
    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!nvs_mutex_lock(portMAX_DELAY))
    {
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CFG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        nvs_mutex_unlock();
        return err;
    }

    uint8_t val = value ? 1 : 0;
    err = nvs_set_u8(handle, key, val);
    if (err != ESP_OK)
    {
        nvs_close(handle);
        nvs_mutex_unlock();
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    nvs_mutex_unlock();
    return err;
}

esp_err_t nvs_get_flag(const char *key, bool *out_value)
{
    if (key == NULL || out_value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!nvs_mutex_lock(portMAX_DELAY))
    {
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CFG_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        nvs_mutex_unlock();
        return err;
    }

    uint8_t val;
    err = nvs_get_u8(handle, key, &val);
    if (err == ESP_OK)
    {
        *out_value = (val != 0);
    }

    nvs_close(handle);
    nvs_mutex_unlock();
    return err;
}

esp_err_t nvs_clear_flag(const char *key)
{
    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!nvs_mutex_lock(portMAX_DELAY))
    {
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CFG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        nvs_mutex_unlock();
        return err;
    }

    err = nvs_erase_key(handle, key);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(handle);
        nvs_mutex_unlock();
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    nvs_mutex_unlock();
    return err;
}

bool nvs_is_flag_set(const char *key)
{
    bool value;
    return nvs_get_flag(key, &value) == ESP_OK && value;
}
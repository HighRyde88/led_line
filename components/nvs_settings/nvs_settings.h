#ifndef NVS_STORAGE_H
#define NVS_STORAGE_H

#include "esp_err.h"
#include "nvs.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Инициализация NVS хранилища
 * 
 * @return esp_err_t ESP_OK при успешной инициализации, иначе код ошибки
 */
esp_err_t nvs_storage_initialization(void);

/**
 * @brief Деинициализация NVS хранилища и освобождение ресурсов
 */
void nvs_storage_deinit(void);

/**
 * @brief Сохранение данных в NVS
 * 
 * @param namespace Пространство имен
 * @param key Ключ данных
 * @param data Указатель на данные для сохранения
 * @param length Размер данных в байтах
 * @param type Тип данных NVS
 * @return esp_err_t ESP_OK при успешном сохранении, иначе код ошибки
 */
esp_err_t nvs_save_data(const char *namespace, const char *key, const void *data, size_t length, nvs_type_t type);

/**
 * @brief Загрузка данных из NVS
 * 
 * @param namespace Пространство имен
 * @param key Ключ данных
 * @param out_data Указатель на буфер для данных (может быть NULL для получения размера)
 * @param length Указатель на размер буфера (при выходе содержит фактический размер)
 * @param type Тип данных NVS
 * @return esp_err_t ESP_OK при успешной загрузке, ESP_ERR_NVS_NOT_FOUND если данные отсутствуют, иначе код ошибки
 */
esp_err_t nvs_load_data(const char *namespace, const char *key, void *out_data, size_t *length, nvs_type_t type);

/**
 * @brief Удаление данных из NVS по ключу
 * 
 * @param namespace Пространство имен
 * @param key Ключ для удаления
 * @return esp_err_t ESP_OK при успешном удалении, иначе код ошибки
 */
esp_err_t nvs_delete_data(const char *namespace, const char *key);

/**
 * @brief Очистка всего пространства имен
 * 
 * @param namespace Пространство имен для очистки
 * @return esp_err_t ESP_OK при успешной очистке, иначе код ошибки
 */
esp_err_t nvs_clear_namespace(const char *namespace);

/**
 * @brief Сохранение булевого флага
 * 
 * @param key Ключ флага
 * @param value Значение флага
 * @return esp_err_t ESP_OK при успешном сохранении, иначе код ошибки
 */
esp_err_t nvs_set_flag(const char *key, bool value);

/**
 * @brief Получение булевого флага
 * 
 * @param key Ключ флага
 * @param out_value Указатель для возвращаемого значения
 * @return esp_err_t ESP_OK при успешном получении, ESP_ERR_NVS_NOT_FOUND если флаг отсутствует, иначе код ошибки
 */
esp_err_t nvs_get_flag(const char *key, bool *out_value);

/**
 * @brief Удаление булевого флага
 * 
 * @param key Ключ флага для удаления
 * @return esp_err_t ESP_OK при успешном удалении, иначе код ошибки
 */
esp_err_t nvs_clear_flag(const char *key);

/**
 * @brief Проверка установлен ли флаг
 * 
 * @param key Ключ флага
 * @return bool true если флаг установлен и равен true, иначе false
 */
bool nvs_is_flag_set(const char *key);

#ifdef __cplusplus
}
#endif

#endif /* NVS_STORAGE_H */
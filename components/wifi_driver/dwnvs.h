#ifndef DWNVS_STORAGE_H
#define DWNVS_STORAGE_H

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Инициализация NVS хранилища
     *
     * @return esp_err_t ESP_OK при успешной инициализации, иначе код ошибки
     */
    esp_err_t dwnvs_storage_initialization(void);

    /**
     * @brief Сохранение конфигурации STA в NVS
     *
     * @param sta_config Указатель на конфигурацию STA
     * @return esp_err_t ESP_OK при успешном сохранении, иначе код ошибки
     */
    esp_err_t dwnvs_save_sta_config(const wifi_sta_config_t *sta_config);

    /**
     * @brief Загрузка конфигурации STA из NVS
     *
     * @param sta_config Указатель на структуру для загрузки конфигурации
     * @return esp_err_t ESP_OK при успешной загрузке, ESP_ERR_NVS_NOT_FOUND если конфигурация отсутствует, иначе код ошибки
     */
    esp_err_t dwnvs_load_sta_config(wifi_sta_config_t *sta_config);

    /**
     * @brief Удаление конфигурации STA из NVS
     *
     * @return esp_err_t ESP_OK при успешном удалении, иначе код ошибки
     */
    esp_err_t dwnvs_delete_sta_config(void);

    /**
     * @brief Сохранение конфигурации AP в NVS
     *
     * @param ap_config Указатель на конфигурацию AP
     * @return esp_err_t ESP_OK при успешном сохранении, иначе код ошибки
     */
    esp_err_t dwnvs_save_ap_config(const wifi_ap_config_t *ap_config);

    /**
     * @brief Загрузка конфигурации AP из NVS
     *
     * @param ap_config Указатель на структуру для загрузки конфигурации
     * @return esp_err_t ESP_OK при успешной загрузке, ESP_ERR_NVS_NOT_FOUND если конфигурация отсутствует, иначе код ошибки
     */
    esp_err_t dwnvs_load_ap_config(wifi_ap_config_t *ap_config);

    /**
     * @brief Удаление конфигурации AP из NVS
     *
     * @return esp_err_t ESP_OK при успешном удалении, иначе код ошибки
     */
    esp_err_t dwnvs_delete_ap_config(void);

    /**
     * @brief Сохранение информации о статическом IP в NVS
     *
     * @param ipinfo Указатель на структуру с IP информацией
     * @return esp_err_t ESP_OK при успешном сохранении, иначе код ошибки
     */
    esp_err_t dwnvs_save_ipinfo(const char *namespace, const esp_netif_ip_info_t *ipinfo);

    /**
     * @brief Загрузка информации о статическом IP из NVS
     *
     * @param ipinfo Указатель на структуру для загрузки IP информации
     * @return esp_err_t ESP_OK при успешной загрузке, ESP_ERR_NVS_NOT_FOUND если информация отсутствует, иначе код ошибки
     */
    esp_err_t dwnvs_load_ipinfo(const char *namespace, esp_netif_ip_info_t *ipinfo);

    /**
     * @brief Удаление информации о статическом IP из NVS
     *
     * @return esp_err_t ESP_OK при успешном удалении, иначе код ошибки
     */
    esp_err_t dwnvs_delete_ipinfo(const char *namespace);

#ifdef __cplusplus
}
#endif

#endif /* DWNVS_STORAGE_H */
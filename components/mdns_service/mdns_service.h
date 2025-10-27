#ifndef MDNS_SERVICE_H
#define MDNS_SERVICE_H

#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "mdns.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Инициализация mDNS сервиса
     * @param hostname - имя хоста
     * @param instance - имя инстанса
     * @return esp_err_t - код ошибки
     */
    esp_err_t init_mdns_service(const char *hostname, const char *instance);

    /**
     * @brief Добавление HTTP сервиса
     * @param service_name - имя сервиса
     * @param port - порт сервиса
     * @return esp_err_t - код ошибки
     */
    esp_err_t add_http_service(const char *service_name, uint16_t port);

    /**
     * @brief Добавление кастомного сервиса
     * @param service_name - имя сервиса
     * @param service_type - тип сервиса (например, "_http")
     * @param proto - протокол (например, "_tcp")
     * @param port - порт сервиса
     * @param txt_data - TXT записи (может быть NULL)
     * @param txt_count - количество TXT записей
     * @return esp_err_t - код ошибки
     */
    esp_err_t add_custom_service(const char *service_name, const char *service_type,
                                 const char *proto, uint16_t port,
                                 mdns_txt_item_t *txt_data, size_t txt_count);

    /**
     * @brief Удаление сервиса
     * @param service_type - тип сервиса
     * @param proto - протокол
     * @return esp_err_t - код ошибки
     */
  esp_err_t remove_service(const char *service_type, const char *proto);

    /**
     * @brief Деинициализация mDNS сервиса
     */
    void deinit_mdns_service(void);

    /**
     * @brief Проверка готовности mDNS сервиса
     * @return bool - true если mDNS инициализирован
     */
    bool is_mdns_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* MDNS_SERVICE_H */
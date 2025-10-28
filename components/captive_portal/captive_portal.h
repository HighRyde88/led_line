#ifndef __CAPTP_H
#define __CAPTP_H

#include "wifi.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Тип колбэка, вызываемого при необходимости подключиться к Wi-Fi как STA
    // Возвращает ESP_OK, если подключение прошло успешно, иначе ошибку
    typedef esp_err_t (*portal_sta_connect_attempt_cb_t)(void);

    /**
     * @brief Запускает captive portal с возможностью вызова пользовательского колбэка для подключения в режиме STA.
     *
     * @param ssid              - SSID для AP (если запускается).
     * @param password          - Пароль для AP (если запускается).
     * @param start_ap          - Флаг: если true, то запускается AP; иначе сначала пытается подключиться как STA.
     * @param try_sta_connect   - Колбэк, вызываемый при необходимости подключиться к Wi-Fi в режиме STA.
     *                            Если NULL, сразу запускается AP (не пытается подключиться как STA).
     */
    void portal_start_with_sta_attempt(const char *ssid, const char *password, bool start_ap, portal_sta_connect_attempt_cb_t try_sta_connect);

    /**
     * @brief Останавливает captive portal и освобождает ресурсы.
     *
     * @param stop_sta - Флаг: если true, останавливает также режим STA.
     */
    void portal_stop(bool stop_sta);

#ifdef __cplusplus
}
#endif

#endif
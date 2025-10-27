#ifndef __SRVR_H
#define __SRVR_H

#include "freertos/FreeRTOS.h"
#include "esp_http_server.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C"
{
#endif

    bool ws_server_send_string(const char *str);

    void captive_portal_dns_server_start(esp_netif_t *netif);
    esp_err_t captive_portal_dns_server_stop(void);

    void captive_portal_http_server_start(esp_netif_t *netif);
    esp_err_t captive_portal_http_server_stop(void);

    void captive_portal_ws_server_start(esp_netif_t *netif);
    esp_err_t captive_portal_ws_server_stop(void);

#ifdef __cplusplus
}
#endif

#endif
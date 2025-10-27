#ifndef __CAPTP_H
#define __CAPTP_H

#include "wifi.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void captive_portal_start(const char *ssid, const char *password, bool start_ap);
    void captive_portal_stop(bool isStopSTA);

#ifdef __cplusplus
}
#endif

#endif
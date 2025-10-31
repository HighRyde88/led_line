#include "cJSON.h"
#include "esp_log.h"
#include "server/server.h"

//=================================================================
void send_response_json(const char *type, const char *target, const char *status, const void *message, bool isObject)
{
    cJSON *response = cJSON_CreateObject();
    if (!response)
    {
        return;
    }

    cJSON_AddStringToObject(response, "type", type);
    if (target)
        cJSON_AddStringToObject(response, "target", target);
    cJSON_AddStringToObject(response, "status", status);

    if (message && !isObject)
    {
        char *message_str = (char *)message;
        cJSON_AddStringToObject(response, "data", message_str);
    }
    else if (message && isObject)
    {
        cJSON *message_obj = (cJSON *)message;
        cJSON_AddItemToObject(response, "data", message_obj);
    }

    char *json_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    if (json_str)
    {
        ws_server_send_string(json_str);
        cJSON_free(json_str);
    }
}
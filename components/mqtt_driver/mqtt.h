#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "mqtt_client.h" // Подключаем esp_mqtt_client для типов, например, esp_event_handler_t
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Структура конфигурации MQTT
     *
     * Содержит параметры, необходимые для инициализации MQTT-клиента.
     * Используется в mqtt_client_start.
     */
    typedef struct
    {
        char *server_uri;           /*!< URI брокера, например "mqtt://host:port" */
        char *username;             /*!< Имя пользователя (может быть NULL) */
        char *password;             /*!< Пароль (может быть NULL) */
        char *client_id;            /*!< Идентификатор клиента (если NULL или пустой, генерируется автоматически) */
        char *lwt_topic;            /*!< Топик Last Will и Testament (может быть NULL) */
        char *lwt_msg;              /*!< Сообщение LWT (если NULL, LWT отключено) */
        int lwt_qos;                /*!< QoS для LWT (по умолчанию 0) */
        int lwt_retain;             /*!< Флаг retain для LWT (по умолчанию 0) */
        int keepalive;              /*!< Интервал keepalive в секундах (по умолчанию 60) */
        bool disable_clean_session; /*!< Отключить clean session (по умолчанию false) */
        int reconnect_timeout;      /*!< Таймаут переподключения в миллисекундах (по умолчанию 500) */
        bool auto_reconnect;        /*!< Включить автоподключение (по умолчанию true) */
    } mqtt_config_t;

    /**
     * @brief Тип данных для обработки событий MQTT
     */
    typedef void *mqtt_client_handle_t; // ✅ Используем void*, т.к. это указатель на mqtt_client_data_t

    /**
     * @brief Инициализация и запуск MQTT-клиента
     *
     * Инициализирует и запускает MQTT-клиент с переданной конфигурацией.
     *
     * @param config Указатель на структуру конфигурации MQTT.
     * @param event_handler Пользовательский обработчик событий MQTT (может быть NULL)
     * @return mqtt_client_handle_t Указатель на клиентский объект или NULL в случае ошибки
     *
     * @note Возвращаемый объект нужно использовать в других функциях.
     */
    mqtt_client_handle_t mqtt_client_start(const mqtt_config_t *config, esp_event_handler_t event_handler);

    /**
     * @brief Остановка MQTT-клиента
     *
     * Останавливает MQTT-клиент, отключается от брокера, уничтожает клиентский
     * контекст.
     *
     * @param client Указатель на клиентский объект
     * @return esp_err_t ESP_OK при успешной остановке, код ошибки в случае неудачи
     */
    esp_err_t mqtt_client_stop(mqtt_client_handle_t client);

    /**
     * @brief Публикация сообщения в топик
     *
     * Потокобезопасно публикует сообщение в указанный топик.
     *
     * @param client Указатель на клиентский объект
     * @param topic Топик, в который публикуется сообщение.
     * @param data Данные сообщения (может быть NULL, если len = 0).
     * @param len Длина данных (0, если data = NULL).
     * @param qos QoS уровня доставки (0, 1 или 2).
     * @param retain Флаг удержания сообщения (retain) на брокере.
     * @return esp_err_t ESP_OK при успешной отправке, код ошибки в случае неудачи
     */
    esp_err_t mqtt_client_publish(mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain);

    /**
     * @brief Подписка на топик
     *
     * Потокобезопасно подписывается на указанный топик.
     *
     * @param client Указатель на клиентский объект
     * @param topic Топик, на который подписываемся.
     * @param qos Максимальный QoS, который будет использоваться для сообщений.
     * @return esp_err_t ESP_OK при успешной подписке, код ошибки в случае неудачи
     */
    esp_err_t mqtt_client_subscribe(mqtt_client_handle_t client, const char *topic, int qos);

    /**
     * @brief Отписка от топика
     *
     * Потокобезопасно отписывается от указанного топика.
     *
     * @param client Указатель на клиентский объект
     * @param topic Топик, от которого отписываемся.
     * @return esp_err_t ESP_OK при успешной отписке, код ошибки в случае неудачи
     */
    esp_err_t mqtt_client_unsubscribe(mqtt_client_handle_t client, const char *topic);

#ifdef __cplusplus
}
#endif

#endif // MQTT_CLIENT_H
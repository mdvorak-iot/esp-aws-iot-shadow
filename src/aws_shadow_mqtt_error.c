#include "aws_shadow_mqtt_error.h"
#include <esp_log.h>

void aws_shadow_log_mqtt_error(const char *tag, const esp_mqtt_error_codes_t *error)
{
    if (error == NULL)
    {
        ESP_LOGW(tag, "unknown error");
        return;
    }

    switch (error->error_type)
    {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)
    case MQTT_ERROR_TYPE_TCP_TRANSPORT:
        ESP_LOGW(TAG, "connection tls error: 0x%x (%s), stack error number 0x%x, last captured errno: %d (%s)",
                 error->esp_tls_last_esp_err,
                 esp_err_to_name(error->esp_tls_last_esp_err),
                 error->esp_tls_stack_err,
                 error->esp_transport_sock_errno,
                 strerror(error->esp_transport_sock_errno));
#else
    case MQTT_ERROR_TYPE_ESP_TLS:
        ESP_LOGW(tag, "connection tls error: 0x%x (%s), stack error number 0x%x",
                 error->esp_tls_last_esp_err,
                 esp_err_to_name(error->esp_tls_last_esp_err),
                 error->esp_tls_stack_err);
#endif
        break;

    case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
        ESP_LOGW(tag, "connection refused error: 0x%x", error->connect_return_code);
        break;

    default:
        ESP_LOGW(tag, "unknown error type: 0x%x", error->error_type);
        break;
    }
}

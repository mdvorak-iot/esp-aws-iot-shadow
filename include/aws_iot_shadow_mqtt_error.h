#ifndef aws_IOT_SHADOW_MQTT_ERROR_H_
#define aws_IOT_SHADOW_MQTT_ERROR_H_

#include <esp_err.h>
#include <mqtt_client.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convenience method, that logs human-readable summary of mqtt error.
 *
 * @param tag Logging tag for logging library
 * @param error MQTT error reference
 */
void aws_iot_shadow_log_mqtt_error(const char *tag, const esp_mqtt_error_codes_t *error);

#ifdef __cplusplus
}
#endif

#endif

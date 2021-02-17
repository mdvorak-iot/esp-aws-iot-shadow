#ifndef ESP_AWS_SHADOW_H_
#define ESP_AWS_SHADOW_H_

#include <esp_err.h>
#include <mqtt_client.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct esp_aws_shadow_handle *esp_aws_shadow_handle_t;

	esp_err_t esp_aws_shadow_init(esp_mqtt_client_handle_t client, const char *thing_name, const char *shadow_name, esp_aws_shadow_handle_t *handle);

	esp_err_t esp_aws_shadow_delete(esp_aws_shadow_handle_t handle);

	bool esp_aws_shadow_is_ready(esp_aws_shadow_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif

#ifndef ESP_AWS_SHADOW_H_
#define ESP_AWS_SHADOW_H_

#include <esp_err.h>
#include <mqtt_client.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct esp_aws_shadow_handle *esp_aws_shadow_handle_t;

	ESP_EVENT_DECLARE_BASE(AWS_SHADOW_EVENT);

	typedef enum
	{
		AWS_SHADOW_EVENT_READY,			/** Connected and initialized */
		AWS_SHADOW_EVENT_DISCONNECTED,	/** Disconnected from the server */
		AWS_SHADOW_EVENT_ERROR,			/** Received error to an action */
		AWS_SHADOW_EVENT_DESIRED_STATE, /** Received updated desired state */
		AWS_SHADOW_EVENT_MAX,			/** Invalid event ID */
	} aws_shadow_event_t;

	typedef struct
	{
		aws_shadow_event_t event_id;
		esp_aws_shadow_handle_t handle;
		const char *thing_name;
		const char *shadow_name;
	} aws_shadow_event_data_t;

	esp_err_t esp_aws_shadow_init(esp_mqtt_client_handle_t client, const char *thing_name, const char *shadow_name, esp_aws_shadow_handle_t *handle);

	esp_err_t esp_aws_shadow_delete(esp_aws_shadow_handle_t handle);

	bool esp_aws_shadow_is_ready(esp_aws_shadow_handle_t handle);

	bool esp_aws_shadow_wait_for_ready(esp_aws_shadow_handle_t handle, TickType_t ticks_to_wait);

#ifdef __cplusplus
}
#endif

#endif

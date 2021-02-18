#ifndef ESP_AWS_SHADOW_H_
#define ESP_AWS_SHADOW_H_

#include <esp_err.h>
#include <mqtt_client.h>
#include <cJSON.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct esp_aws_shadow_handle *esp_aws_shadow_handle_t;

	ESP_EVENT_DECLARE_BASE(AWS_SHADOW_EVENT);

	typedef enum
	{
		AWS_SHADOW_EVENT_ANY = ESP_EVENT_ANY_ID, /** Handle any event */
		AWS_SHADOW_EVENT_READY = 0,				 /** Connected and initialized */
		AWS_SHADOW_EVENT_DISCONNECTED,			 /** Disconnected from the server */
		AWS_SHADOW_EVENT_ERROR,					 /** Received error to an action */
		AWS_SHADOW_EVENT_UPDATE_ACCEPTED,		 /** Received an updated state */
		AWS_SHADOW_EVENT_UPDATE_DELTA,			 /** Received a state delta */
		AWS_SHADOW_EVENT_DELETE_ACCEPTED,		 /** Shadow was deleted */
		AWS_SHADOW_EVENT_MAX,					 /** Invalid event ID */
	} aws_shadow_event_t;

	typedef struct
	{
		int code;
		const char *message;
	} aws_shadow_event_error_t;

	typedef struct
	{
		aws_shadow_event_t event_id;
		esp_aws_shadow_handle_t handle;
		const char *thing_name;
		const char *shadow_name;
		const cJSON *root;
		const cJSON *desired;
		const cJSON *reported;
		const cJSON *delta;
		const char *client_token;
		const aws_shadow_event_error_t *error;
	} aws_shadow_event_data_t;

	esp_err_t esp_aws_shadow_init(esp_mqtt_client_handle_t client, const char *thing_name, const char *shadow_name, esp_aws_shadow_handle_t *handle);

	esp_err_t esp_aws_shadow_delete(esp_aws_shadow_handle_t handle);

	esp_err_t esp_aws_shadow_handler_register(esp_aws_shadow_handle_t handle, aws_shadow_event_t event_id, esp_event_handler_t event_handler, void *event_handler_arg);

	esp_err_t esp_aws_shadow_handler_instance_register(esp_aws_shadow_handle_t handle, aws_shadow_event_t event_id, esp_event_handler_t event_handler, void *event_handler_arg, esp_event_handler_instance_t *handler_ctx_arg);

	esp_err_t esp_aws_shadow_handler_instance_unregister(esp_aws_shadow_handle_t handle, aws_shadow_event_t event_id, esp_event_handler_instance_t handler_ctx_arg);

	bool esp_aws_shadow_is_ready(esp_aws_shadow_handle_t handle);

	bool esp_aws_shadow_wait_for_ready(esp_aws_shadow_handle_t handle, TickType_t ticks_to_wait);

	esp_err_t esp_aws_shadow_request_update(esp_aws_shadow_handle_t handle, const cJSON *root);

	esp_err_t esp_aws_shadow_request_update_reported(esp_aws_shadow_handle_t handle, const cJSON *reported, const char *client_token);

	esp_err_t esp_aws_shadow_request_update_desired(esp_aws_shadow_handle_t handle, const cJSON *desired, const char *client_token);

	esp_err_t esp_aws_shadow_request_delete(esp_aws_shadow_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif

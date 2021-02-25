#ifndef AWS_IOT_SHADOW_H_
#define AWS_IOT_SHADOW_H_

#include <cJSON.h>
#include <esp_err.h>
#include <mqtt_client.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AWS_IOT_SHADOW_JSON_STATE "state"
#define AWS_IOT_SHADOW_JSON_DESIRED "desired"
#define AWS_IOT_SHADOW_JSON_REPORTED "reported"
#define AWS_IOT_SHADOW_JSON_DELTA "delta"
#define AWS_IOT_SHADOW_JSON_VERSION "version"
#define AWS_IOT_SHADOW_JSON_CLIENT_TOKEN "clientToken"
#define AWS_IOT_SHADOW_JSON_MESSAGE "message"
#define AWS_IOT_SHADOW_JSON_CODE "code"

#ifndef AWS_IOT_SHADOW_SUPPORT_DELTA
#define AWS_IOT_SHADOW_SUPPORT_DELTA CONFIG_AWS_IOT_SHADOW_SUPPORT_DELTA
#endif

ESP_EVENT_DECLARE_BASE(AWS_IOT_SHADOW_EVENT);

typedef struct aws_iot_shadow_handle *aws_iot_shadow_handle_ptr;

/**
 * @brief Event types for a shadow.
 *
 * Note that handlers are dispatched on custom event loop, therefore
 * they be registered via aws_iot_shadow_handler_register() and not default
 * functions.
 *
 * @see aws_iot_shadow_handler_register
 * @see aws_iot_shadow_handler_instance_register
 */
enum aws_iot_shadow_event
{
    /** @brief Handle any event */
    AWS_IOT_SHADOW_EVENT_ANY = ESP_EVENT_ANY_ID,
    /** @brief Connected and initialized */
    AWS_IOT_SHADOW_EVENT_READY = 0,
    /** @brief Disconnected from the server */
    AWS_IOT_SHADOW_EVENT_DISCONNECTED = 1,
    /** @brief Received error to an action */
    AWS_IOT_SHADOW_EVENT_ERROR = 2,
    /** @brief Received a get state */
    AWS_IOT_SHADOW_EVENT_GET_ACCEPTED = 3,
    /** @brief Shadow was deleted */
    AWS_IOT_SHADOW_EVENT_DELETE_ACCEPTED = 4,
    /** @brief Received an updated state */
    AWS_IOT_SHADOW_EVENT_UPDATE_ACCEPTED = 5,
#if AWS_IOT_SHADOW_SUPPORT_DELTA
    /** @brief Received a state delta */
    AWS_IOT_SHADOW_EVENT_UPDATE_DELTA = 6,
#endif
    /** Invalid event ID */
    AWS_IOT_SHADOW_EVENT_MAX = 7,
};

struct aws_iot_shadow_event_error
{
    int code;
    const char *message;
};

struct aws_iot_shadow_event_data
{
    enum aws_iot_shadow_event event_id;
    aws_iot_shadow_handle_ptr handle;
    const char *thing_name;
    const char *shadow_name;
    const cJSON *root;
    const cJSON *desired;
    const cJSON *reported;
    const cJSON *delta;
    const char *client_token;
    const struct aws_iot_shadow_event_error *error;
};

esp_err_t aws_iot_shadow_init(esp_mqtt_client_handle_t client, const char *thing_name, const char *shadow_name,
                              aws_iot_shadow_handle_ptr *handle);

esp_err_t aws_iot_shadow_delete(aws_iot_shadow_handle_ptr handle);

/**
 * @brief Find a thing name from a MQTT client_id.
 *
 * @param client_id MQTT client ID in format `arn:aws:iot:region:AWS-account-ID:thing/Thing-name`.
 * @return Pointer to thing name start in client_id or NULL if not found.
 * Does not allocate new string, references original client_id param.
 */
const char *aws_iot_shadow_thing_name(const char *client_id);

esp_err_t aws_iot_shadow_handler_register(aws_iot_shadow_handle_ptr handle, enum aws_iot_shadow_event event_id,
                                          esp_event_handler_t event_handler, void *event_handler_arg);

esp_err_t aws_iot_shadow_handler_instance_register(aws_iot_shadow_handle_ptr handle, enum aws_iot_shadow_event event_id,
                                                   esp_event_handler_t event_handler, void *event_handler_arg,
                                                   esp_event_handler_instance_t *handler_ctx_arg);

esp_err_t aws_iot_shadow_handler_instance_unregister(aws_iot_shadow_handle_ptr handle, enum aws_iot_shadow_event event_id,
                                                     esp_event_handler_instance_t handler_ctx_arg);

bool aws_iot_shadow_is_ready(aws_iot_shadow_handle_ptr handle);

bool aws_iot_shadow_wait_for_ready(aws_iot_shadow_handle_ptr handle, TickType_t ticks_to_wait);

esp_err_t aws_iot_shadow_request_get(aws_iot_shadow_handle_ptr handle);

esp_err_t aws_iot_shadow_request_update_raw(aws_iot_shadow_handle_ptr handle, const cJSON *root);

esp_err_t aws_iot_shadow_request_update(aws_iot_shadow_handle_ptr handle,
                                        const cJSON *desired,
                                        const cJSON *reported,
                                        const char *client_token);

esp_err_t aws_iot_shadow_request_update_reported(aws_iot_shadow_handle_ptr handle,
                                                 const cJSON *reported,
                                                 const char *client_token);

esp_err_t aws_iot_shadow_request_delete(aws_iot_shadow_handle_ptr handle);

#ifdef __cplusplus
}
#endif

#endif

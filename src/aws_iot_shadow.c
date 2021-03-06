#include "aws_iot_shadow.h"
#include "aws_iot_shadow_topic.h"
#include <esp_event.h>
#include <esp_log.h>
#include <freertos/event_groups.h>
#include <string.h>

static const char TAG[] = "aws_iot_shadow";

ESP_EVENT_DEFINE_BASE(AWS_IOT_SHADOW_EVENT);

#define AWS_IOT_SHADOW_EVENT_DATA_INITIALIZER(handle_, event_id_) \
    {                                                             \
        .event_id = (event_id_),                                  \
        .handle = (handle_),                                      \
        .thing_name = (handle_)->thing_name,                      \
        .shadow_name = (handle_)->shadow_name,                    \
        .data = NULL,                                             \
        .data_len = 0,                                            \
    }

static const int CONNECTED_BIT = BIT0;
static const int SUBSCRIBED_GET_ACCEPTED_BIT = BIT12;
static const int SUBSCRIBED_GET_REJECTED_BIT = BIT13;
static const int SUBSCRIBED_UPDATE_ACCEPTED_BIT = BIT14;
static const int SUBSCRIBED_UPDATE_REJECTED_BIT = BIT15;
#if AWS_IOT_SHADOW_SUPPORT_DELTA
static const int SUBSCRIBED_UPDATE_DELTA_BIT = BIT16;
#else
static const int SUBSCRIBED_UPDATE_DELTA_BIT = 0; // no bit, used in SUBSCRIBED_ALL_BITS
#endif
static const int SUBSCRIBED_DELETE_ACCEPTED_BIT = BIT17;
static const int SUBSCRIBED_DELETE_REJECTED_BIT = BIT18;

static const int SUBSCRIBED_ALL_BITS =
    SUBSCRIBED_GET_ACCEPTED_BIT | SUBSCRIBED_GET_REJECTED_BIT | SUBSCRIBED_UPDATE_ACCEPTED_BIT | SUBSCRIBED_UPDATE_REJECTED_BIT | SUBSCRIBED_UPDATE_DELTA_BIT | SUBSCRIBED_DELETE_ACCEPTED_BIT | SUBSCRIBED_DELETE_REJECTED_BIT;

struct aws_iot_shadow_handle
{
    esp_mqtt_client_handle_t client;
    esp_event_loop_handle_t event_loop;
    EventGroupHandle_t event_group;
    char topic_prefix[AWS_IOT_SHADOW_TOPIC_MAX_LENGTH];
    uint8_t topic_prefix_len;

    char thing_name[AWS_IOT_SHADOW_THINGNAME_LENGTH_MAX];
    char shadow_name[AWS_IOT_SHADOW_NAME_LENGTH_MAX];

    // For MQTT_EVENT_SUBSCRIBED tracking
    struct topic_subscriptions_t
    {
        int get_accepted_msg_id;
        int get_rejected_msg_id;
        int update_accepted_msg_id;
        int update_rejected_msg_id;
        int delete_accepted_msg_id;
        int delete_rejected_msg_id;
#if AWS_IOT_SHADOW_SUPPORT_DELTA
        int update_delta_msg_id;
#endif
    } topic_subscriptions;
};

inline static char *aws_iot_shadow_topic_name(aws_iot_shadow_handle_ptr handle, const char *topic_suffix,
                                              char *topic_buf, uint16_t topic_buf_len)
{
    assert(handle);
    assert(topic_suffix);
    assert(topic_buf);
    assert(topic_buf_len > 0);

    size_t topic_suffix_len = strlen(topic_suffix);

    // Validate size (include terminating \0 char), to prevent buffer overflow
    if (handle->topic_prefix_len + topic_suffix_len + 1 >= topic_buf_len)
    {
        topic_buf[0] = '\0';
        return NULL;
    }

    memcpy(topic_buf, handle->topic_prefix, handle->topic_prefix_len);
    memcpy(topic_buf + handle->topic_prefix_len, topic_suffix, topic_suffix_len + 1); // topic_suffix_len does not include terminating null char
    ESP_LOGD(TAG, "topic name: '%s'", topic_buf);
    return topic_buf;
}

static void aws_iot_shadow_event_dispatch(aws_iot_shadow_handle_ptr handle,
                                          enum aws_iot_shadow_event event_id,
                                          esp_mqtt_event_handle_t mqtt_event)
{
    // Prepare event
    struct aws_iot_shadow_event_data shadow_event = AWS_IOT_SHADOW_EVENT_DATA_INITIALIZER(handle, event_id);

    if (mqtt_event)
    {
        shadow_event.data = mqtt_event->data;
        shadow_event.data_len = mqtt_event->data_len;
    }

    // Add to queue
    ESP_LOGD(TAG, "dispatching event %d for %s", shadow_event.event_id, handle->topic_prefix);
    esp_err_t err = esp_event_post_to(handle->event_loop, AWS_IOT_SHADOW_EVENT, shadow_event.event_id, &shadow_event, sizeof(shadow_event), portMAX_DELAY);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_event_post_to failed: %d", err);
        return;
    }

    // Run event loop
    err = esp_event_loop_run(handle->event_loop, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to dispatch event %d: %d (%s)", shadow_event.event_id, err, esp_err_to_name(err));
        return;
    }
}

static void aws_iot_shadow_mqtt_connected(aws_iot_shadow_handle_ptr handle)
{
    // Reset tracking
    xEventGroupClearBits(handle->event_group, SUBSCRIBED_ALL_BITS);
    memset(&handle->topic_subscriptions, 0, sizeof(handle->topic_subscriptions));

    // Subscribe
    char topic_name[AWS_IOT_SHADOW_TOPIC_MAX_LENGTH] = {};

    handle->topic_subscriptions.get_accepted_msg_id = esp_mqtt_client_subscribe(handle->client, aws_iot_shadow_topic_name(handle, (AWS_IOT_SHADOW_OP_GET AWS_IOT_SHADOW_SUFFIX_ACCEPTED), topic_name, sizeof(topic_name)), 0);
    handle->topic_subscriptions.get_rejected_msg_id = esp_mqtt_client_subscribe(handle->client, aws_iot_shadow_topic_name(handle, (AWS_IOT_SHADOW_OP_GET AWS_IOT_SHADOW_SUFFIX_REJECTED), topic_name, sizeof(topic_name)), 0);
    handle->topic_subscriptions.update_accepted_msg_id = esp_mqtt_client_subscribe(handle->client, aws_iot_shadow_topic_name(handle, (AWS_IOT_SHADOW_OP_UPDATE AWS_IOT_SHADOW_SUFFIX_ACCEPTED), topic_name, sizeof(topic_name)), 0);
    handle->topic_subscriptions.update_rejected_msg_id = esp_mqtt_client_subscribe(handle->client, aws_iot_shadow_topic_name(handle, (AWS_IOT_SHADOW_OP_UPDATE AWS_IOT_SHADOW_SUFFIX_REJECTED), topic_name, sizeof(topic_name)), 0);
    handle->topic_subscriptions.delete_accepted_msg_id = esp_mqtt_client_subscribe(handle->client, aws_iot_shadow_topic_name(handle, (AWS_IOT_SHADOW_OP_DELETE AWS_IOT_SHADOW_SUFFIX_ACCEPTED), topic_name, sizeof(topic_name)), 0);
    handle->topic_subscriptions.delete_rejected_msg_id = esp_mqtt_client_subscribe(handle->client, aws_iot_shadow_topic_name(handle, (AWS_IOT_SHADOW_OP_DELETE AWS_IOT_SHADOW_SUFFIX_REJECTED), topic_name, sizeof(topic_name)), 0);
#if AWS_IOT_SHADOW_SUPPORT_DELTA
    handle->topic_subscriptions.update_delta_msg_id = esp_mqtt_client_subscribe(handle->client, aws_iot_shadow_topic_name(handle, (AWS_IOT_SHADOW_OP_UPDATE AWS_IOT_SHADOW_SUFFIX_DELTA), topic_name, sizeof(topic_name)), 0);
#endif

    // Connected state
    xEventGroupSetBits(handle->event_group, CONNECTED_BIT);
    ESP_LOGI(TAG, "%s connected to mqtt server", handle->topic_prefix);
}

static void aws_iot_shadow_mqtt_disconnected(aws_iot_shadow_handle_ptr handle)
{
    xEventGroupClearBits(handle->event_group, CONNECTED_BIT | SUBSCRIBED_ALL_BITS);
    aws_iot_shadow_event_dispatch(handle, AWS_IOT_SHADOW_EVENT_DISCONNECTED, NULL);
}

static void aws_iot_shadow_mqtt_subscribed(aws_iot_shadow_handle_ptr handle, esp_mqtt_event_handle_t event)
{
    EventBits_t bits = 0;

    if (event->msg_id == -1)
    {
        ESP_LOGD(TAG, "invalid subscription msg_id");
        return;
    }
    else if (event->msg_id == handle->topic_subscriptions.get_accepted_msg_id)
    {
        ESP_LOGD(TAG, "subscribed to %s" AWS_IOT_SHADOW_OP_GET AWS_IOT_SHADOW_SUFFIX_ACCEPTED, handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_GET_ACCEPTED_BIT);
    }
    else if (event->msg_id == handle->topic_subscriptions.get_rejected_msg_id)
    {
        ESP_LOGD(TAG, "subscribed to %s" AWS_IOT_SHADOW_OP_GET AWS_IOT_SHADOW_SUFFIX_REJECTED, handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_GET_REJECTED_BIT);
    }
    else if (event->msg_id == handle->topic_subscriptions.update_accepted_msg_id)
    {
        ESP_LOGD(TAG, "subscribed to %s" AWS_IOT_SHADOW_OP_UPDATE AWS_IOT_SHADOW_SUFFIX_ACCEPTED, handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_UPDATE_ACCEPTED_BIT);
    }
    else if (event->msg_id == handle->topic_subscriptions.update_rejected_msg_id)
    {
        ESP_LOGD(TAG, "subscribed to %s" AWS_IOT_SHADOW_OP_UPDATE AWS_IOT_SHADOW_SUFFIX_REJECTED, handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_UPDATE_REJECTED_BIT);
    }
    else if (event->msg_id == handle->topic_subscriptions.delete_accepted_msg_id)
    {
        ESP_LOGD(TAG, "subscribed to %s" AWS_IOT_SHADOW_OP_DELETE AWS_IOT_SHADOW_SUFFIX_ACCEPTED, handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_DELETE_ACCEPTED_BIT);
    }
    else if (event->msg_id == handle->topic_subscriptions.delete_rejected_msg_id)
    {
        ESP_LOGD(TAG, "subscribed to %s" AWS_IOT_SHADOW_OP_DELETE AWS_IOT_SHADOW_SUFFIX_REJECTED, handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_DELETE_REJECTED_BIT);
    }
#if AWS_IOT_SHADOW_SUPPORT_DELTA
    else if (event->msg_id == handle->topic_subscriptions.update_delta_msg_id)
    {
        ESP_LOGD(TAG, "subscribed to %s" AWS_IOT_SHADOW_OP_UPDATE AWS_IOT_SHADOW_SUFFIX_DELTA, handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_UPDATE_DELTA_BIT);
    }
#endif

    // Ready?
    if ((bits & SUBSCRIBED_ALL_BITS) == SUBSCRIBED_ALL_BITS)
    {
        ESP_LOGI(TAG, "%s is ready", handle->topic_prefix);

        aws_iot_shadow_event_dispatch(handle, AWS_IOT_SHADOW_EVENT_READY, NULL);

        // Request data
        esp_err_t err = aws_iot_shadow_request_get(handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "failed to publish %s" AWS_IOT_SHADOW_OP_GET, handle->topic_prefix);
        }
    }
}

static void aws_iot_shadow_mqtt_data_get_op(aws_iot_shadow_handle_ptr handle, esp_mqtt_event_handle_t event,
                                            const char *action, uint16_t action_len)
{
    const char *op = action + AWS_IOT_SHADOW_OP_GET_LENGTH;
    uint16_t op_len = action_len - AWS_IOT_SHADOW_OP_GET_LENGTH;

    if (op_len == AWS_IOT_SHADOW_SUFFIX_ACCEPTED_LENGTH
        && strncmp(op, AWS_IOT_SHADOW_SUFFIX_ACCEPTED, AWS_IOT_SHADOW_SUFFIX_ACCEPTED_LENGTH) == 0)
    {
        // /get/accepted
        aws_iot_shadow_event_dispatch(handle, AWS_IOT_SHADOW_EVENT_GET_ACCEPTED, event);
    }
    else if (op_len == AWS_IOT_SHADOW_SUFFIX_REJECTED_LENGTH
             && strncmp(op, AWS_IOT_SHADOW_SUFFIX_REJECTED, AWS_IOT_SHADOW_SUFFIX_REJECTED_LENGTH) == 0)
    {
        // /get/rejected
        aws_iot_shadow_event_dispatch(handle, AWS_IOT_SHADOW_EVENT_GET_REJECTED, event);
    }
}

static void aws_iot_shadow_mqtt_data_update_op(aws_iot_shadow_handle_ptr handle, esp_mqtt_event_handle_t event,
                                               const char *action, uint16_t action_len)
{
    const char *op = action + AWS_IOT_SHADOW_OP_UPDATE_LENGTH;
    uint16_t op_len = action_len - AWS_IOT_SHADOW_OP_UPDATE_LENGTH;

    if (op_len == AWS_IOT_SHADOW_SUFFIX_ACCEPTED_LENGTH
        && strncmp(op, AWS_IOT_SHADOW_SUFFIX_ACCEPTED, AWS_IOT_SHADOW_SUFFIX_ACCEPTED_LENGTH) == 0)
    {
        // /update/accepted
        aws_iot_shadow_event_dispatch(handle, AWS_IOT_SHADOW_EVENT_UPDATE_ACCEPTED, event);
    }
    else if (op_len == AWS_IOT_SHADOW_SUFFIX_REJECTED_LENGTH
             && strncmp(op, AWS_IOT_SHADOW_SUFFIX_REJECTED, AWS_IOT_SHADOW_SUFFIX_REJECTED_LENGTH) == 0)
    {
        // /update/rejected
        aws_iot_shadow_event_dispatch(handle, AWS_IOT_SHADOW_EVENT_UPDATE_REJECTED, event);
    }
#if AWS_IOT_SHADOW_SUPPORT_DELTA
    else if (op_len == AWS_IOT_SHADOW_SUFFIX_DELTA_LENGTH && strncmp(op, AWS_IOT_SHADOW_SUFFIX_DELTA, AWS_IOT_SHADOW_SUFFIX_DELTA_LENGTH) == 0)
    {
        // /update/delta
        aws_iot_shadow_event_dispatch(handle, AWS_IOT_SHADOW_EVENT_UPDATE_DELTA, event);
    }
#endif
}

static void aws_iot_shadow_mqtt_data_delete_op(aws_iot_shadow_handle_ptr handle, esp_mqtt_event_handle_t event,
                                               const char *action, uint16_t action_len)
{
    const char *op = action + AWS_IOT_SHADOW_OP_DELETE_LENGTH;
    uint16_t op_len = action_len - AWS_IOT_SHADOW_OP_DELETE_LENGTH;

    if (op_len == AWS_IOT_SHADOW_SUFFIX_ACCEPTED_LENGTH
        && strncmp(op, AWS_IOT_SHADOW_SUFFIX_ACCEPTED, AWS_IOT_SHADOW_SUFFIX_ACCEPTED_LENGTH) == 0)
    {
        // /delete/accepted
        aws_iot_shadow_event_dispatch(handle, AWS_IOT_SHADOW_EVENT_DELETE_ACCEPTED, event);
    }
    else if (op_len == AWS_IOT_SHADOW_SUFFIX_REJECTED_LENGTH
             && strncmp(op, AWS_IOT_SHADOW_SUFFIX_REJECTED, AWS_IOT_SHADOW_SUFFIX_REJECTED_LENGTH) == 0)
    {
        // /delete/rejected
        aws_iot_shadow_event_dispatch(handle, AWS_IOT_SHADOW_EVENT_DELETE_REJECTED, event);
    }
}

static void aws_iot_shadow_mqtt_data(aws_iot_shadow_handle_ptr handle, esp_mqtt_event_handle_t event)
{
    ESP_LOGD(TAG, "received %.*s payload (%d bytes): %.*s", event->topic_len, event->topic, event->data_len, event->data_len, event->data ? event->data : "");

    if (event->total_data_len > event->data_len)
    {
        ESP_LOGE(TAG, "received partial data, this is not supported, please increase esp_mqtt_client_config_t.buffer_size to > %d (or set CONFIG_MQTT_BUFFER_SIZE)", event->total_data_len);
        return;
    }

    if (event->topic_len > handle->topic_prefix_len && event->topic_len < AWS_IOT_SHADOW_TOPIC_MAX_LENGTH
        && strncmp(event->topic, handle->topic_prefix, handle->topic_prefix_len) == 0)
    {
        const char *action = event->topic + handle->topic_prefix_len;
        uint16_t action_len = event->topic_len - handle->topic_prefix_len;

        ESP_LOGI(TAG, "%s action %.*s (%d bytes)", handle->topic_prefix, action_len, action, event->total_data_len);

        if (action_len >= AWS_IOT_SHADOW_OP_GET_LENGTH && strncmp(action, AWS_IOT_SHADOW_OP_GET, AWS_IOT_SHADOW_OP_GET_LENGTH) == 0)
        {
            // Get operation
            aws_iot_shadow_mqtt_data_get_op(handle, event, action, action_len);
        }
        else if (action_len >= AWS_IOT_SHADOW_OP_UPDATE_LENGTH
                 && strncmp(action, AWS_IOT_SHADOW_OP_UPDATE, AWS_IOT_SHADOW_OP_UPDATE_LENGTH) == 0)
        {
            // Update operation
            aws_iot_shadow_mqtt_data_update_op(handle, event, action, action_len);
        }
        else if (action_len >= AWS_IOT_SHADOW_OP_DELETE_LENGTH
                 && strncmp(action, AWS_IOT_SHADOW_OP_DELETE, AWS_IOT_SHADOW_OP_DELETE_LENGTH) == 0)
        {
            // Delete operation
            aws_iot_shadow_mqtt_data_delete_op(handle, event, action, action_len);
        }
    }
}

static void aws_iot_shadow_mqtt_handler(void *handler_args, __unused esp_event_base_t base, __unused int32_t event_id, void *event_data)
{
    aws_iot_shadow_handle_ptr handle = (aws_iot_shadow_handle_ptr)handler_args;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        aws_iot_shadow_mqtt_connected(handle);
        break;

    case MQTT_EVENT_DISCONNECTED:
        aws_iot_shadow_mqtt_disconnected(handle);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        aws_iot_shadow_mqtt_subscribed(handle, event);
        break;

    case MQTT_EVENT_DATA:
        aws_iot_shadow_mqtt_data(handle, event);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGD(TAG, "got mqtt error type: %d", event->error_handle->error_type);
        break;

    default:
        ESP_LOGD(TAG, "unhandled mqtt event %d", event->event_id);
        break;
    }
}

const char *aws_iot_shadow_thing_name(const char *client_id)
{
    if (client_id == NULL)
    {
        return NULL;
    }

    // Parse thing_name
    const char *thing_name = strstr(client_id, AWS_IOT_SHADOW_THING_NAME_PREFIX);
    if (thing_name != NULL)
    {
        // strstr returns pointer to searched string start
        thing_name += strlen(AWS_IOT_SHADOW_THING_NAME_PREFIX);
    }
    return thing_name;
}

esp_err_t aws_iot_shadow_init(esp_mqtt_client_handle_t client, const char *thing_name, const char *shadow_name,
                              aws_iot_shadow_handle_ptr *handle)
{
    if (client == NULL || thing_name == NULL || handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    size_t thing_name_len = strlen(thing_name);
    size_t shadow_name_len = shadow_name != NULL ? strlen(shadow_name) : 0;

    if (thing_name_len == 0 || thing_name_len >= AWS_IOT_SHADOW_THINGNAME_LENGTH_MAX
        || shadow_name_len >= AWS_IOT_SHADOW_NAME_LENGTH_MAX)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Alloc
    aws_iot_shadow_handle_ptr result = (aws_iot_shadow_handle_ptr)malloc(sizeof(*result));
    if (result == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    // Init
    memset(result, 0, sizeof(*result));

    result->client = client;
    result->event_group = xEventGroupCreate();
    assert(result->event_group);

    esp_event_loop_args_t event_loop_args = {
        .queue_size = 1,
    };
    esp_err_t err = esp_event_loop_create(&event_loop_args, &result->event_loop);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to create event loop: %d", err);
        aws_iot_shadow_delete(result);
        return err;
    }

    strcpy(result->thing_name, thing_name);

    if (shadow_name == NULL)
    {
        // Classic
        snprintf(result->topic_prefix, sizeof(result->topic_prefix), AWS_IOT_SHADOW_PREFIX_CLASSIC_FORMAT, thing_name);
    }
    else
    {
        // Named
        strcpy(result->shadow_name, shadow_name);
        snprintf(result->topic_prefix, sizeof(result->topic_prefix), AWS_IOT_SHADOW_PREFIX_NAMED_FORMAT, thing_name, shadow_name);
    }

    result->topic_prefix_len = strlen(result->topic_prefix);
    if (result->topic_prefix_len == 0)
    {
        ESP_LOGE(TAG, "failed to format topic prefix for '%s' '%s'", thing_name, shadow_name ? shadow_name : "");
        aws_iot_shadow_delete(result);
        return ESP_FAIL;
    }

    // Handler
    err = esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, aws_iot_shadow_mqtt_handler, result);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to register mqtt event handler: %d", err);
        aws_iot_shadow_delete(result);
        return err;
    }

    // Success
    *handle = result;
    ESP_LOGI(TAG, "initialized %s", result->topic_prefix);
    return ESP_OK;
}

esp_err_t aws_iot_shadow_delete(aws_iot_shadow_handle_ptr handle)
{
    if (handle == NULL)
    {
        return ESP_OK;
    }

    // Unregister event handler
    // TODO esp_mqtt_client_unregister_event is not implemented, without it there is a leak, but in real life this won't be used anyway
    // esp_err_t err = esp_mqtt_client_unregister_event(client, MQTT_EVENT_ANY, aws_iot_shadow_mqtt_handler, result);
    // if (err != ESP_OK)
    // {
    //     ESP_LOGW(TAG, "failed to unregister event handler: %d", err);
    // }

    // Properly destroy
    if (handle->event_group)
    {
        vEventGroupDelete(handle->event_group);
    }

    // Release handle
    free(handle);

    // Success
    return ESP_OK;
}

inline esp_err_t aws_iot_shadow_handler_register(aws_iot_shadow_handle_ptr handle, enum aws_iot_shadow_event event_id,
                                                 esp_event_handler_t event_handler, void *event_handler_arg)
{
    return aws_iot_shadow_handler_instance_register(handle, event_id, event_handler, event_handler_arg, NULL);
}

inline esp_err_t aws_iot_shadow_handler_instance_register(aws_iot_shadow_handle_ptr handle, enum aws_iot_shadow_event event_id,
                                                          esp_event_handler_t event_handler, void *event_handler_arg,
                                                          esp_event_handler_instance_t *handler_ctx_arg)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_event_handler_instance_register_with(handle->event_loop, AWS_IOT_SHADOW_EVENT, event_id,
                                                    event_handler, event_handler_arg, handler_ctx_arg);
}

inline esp_err_t aws_iot_shadow_handler_instance_unregister(aws_iot_shadow_handle_ptr handle, enum aws_iot_shadow_event event_id,
                                                            esp_event_handler_instance_t handler_ctx_arg)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_event_handler_instance_unregister_with(handle->event_loop, AWS_IOT_SHADOW_EVENT, event_id, handler_ctx_arg);
}

bool aws_iot_shadow_is_ready(aws_iot_shadow_handle_ptr handle)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    EventBits_t bits = xEventGroupGetBits(handle->event_group);
    return (bits & SUBSCRIBED_ALL_BITS) == SUBSCRIBED_ALL_BITS;
}

bool aws_iot_shadow_wait_for_ready(aws_iot_shadow_handle_ptr handle, TickType_t ticks_to_wait)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    EventBits_t bits = xEventGroupWaitBits(handle->event_group, SUBSCRIBED_ALL_BITS, pdFALSE, pdTRUE, ticks_to_wait);
    return (bits & SUBSCRIBED_ALL_BITS) == SUBSCRIBED_ALL_BITS;
}

esp_err_t aws_iot_shadow_request_get(aws_iot_shadow_handle_ptr handle)
{
    char topic_name[AWS_IOT_SHADOW_TOPIC_MAX_LENGTH] = {};
    if (aws_iot_shadow_topic_name(handle, AWS_IOT_SHADOW_OP_GET, topic_name, sizeof(topic_name)) == NULL)
    {
        return ESP_ERR_INVALID_SIZE; // buffer overflow
    }

    ESP_LOGI(TAG, "sending %s", topic_name);
    int msg_id = esp_mqtt_client_publish(handle->client, topic_name, NULL, 0, 1, 0);
    return msg_id != -1 ? ESP_OK : ESP_FAIL;
}

esp_err_t aws_iot_shadow_request_delete(aws_iot_shadow_handle_ptr handle)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char topic_name[AWS_IOT_SHADOW_TOPIC_MAX_LENGTH] = {};
    if (aws_iot_shadow_topic_name(handle, AWS_IOT_SHADOW_OP_DELETE, topic_name, sizeof(topic_name)) == NULL)
    {
        return ESP_ERR_INVALID_SIZE; // buffer overflow
    }

    ESP_LOGI(TAG, "sending %s", topic_name);
    int msg_id = esp_mqtt_client_publish(handle->client, topic_name, NULL, 0, 1, 0);
    return msg_id != -1 ? ESP_OK : ESP_FAIL;
}

esp_err_t aws_iot_shadow_request_update(aws_iot_shadow_handle_ptr handle, const char *data, size_t data_len)
{
    if (handle == NULL || data == NULL || data_len > INT_MAX)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char topic_name[AWS_IOT_SHADOW_TOPIC_MAX_LENGTH] = {};
    if (aws_iot_shadow_topic_name(handle, AWS_IOT_SHADOW_OP_UPDATE, topic_name, sizeof(topic_name)) == NULL)
    {
        return ESP_ERR_INVALID_SIZE; // buffer overflow
    }

    ESP_LOGI(TAG, "sending %s (%zu bytes)", topic_name, data_len);
    ESP_LOGD(TAG, "sending %s payload: %.*s", topic_name, (int)data_len, data);

    int msg_id = esp_mqtt_client_publish(handle->client, topic_name, data, data_len, 1, 0);
    return msg_id != -1 ? ESP_OK : ESP_FAIL;
}

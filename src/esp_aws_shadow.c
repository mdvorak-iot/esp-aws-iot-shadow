#include "esp_aws_shadow.h"
#include "esp_aws_shadow_constants.h"
#include "esp_aws_shadow_json.h"
#include <string.h>
#include <esp_log.h>
#include <esp_event.h>
#include <freertos/event_groups.h>
#include <cJSON.h>

static const char TAG[] = "esp_aws_shadow";

ESP_EVENT_DEFINE_BASE(AWS_SHADOW_EVENT);

#define AWS_SHADOW_EVENT_DATA_INITIALIZER(handle_, event_id_) \
    {                                                         \
        .event_id = event_id_,                                \
        .handle = handle_,                                    \
        .thing_name = handle->thing_name,                     \
        .shadow_name = handle->shadow_name,                   \
    }

static const int CONNECTED_BIT = BIT0;
static const int SUBSCRIBED_GET_ACCEPTED_BIT = BIT12;
static const int SUBSCRIBED_GET_REJECTED_BIT = BIT13;
static const int SUBSCRIBED_UPDATE_ACCEPTED_BIT = BIT14;
static const int SUBSCRIBED_UPDATE_REJECTED_BIT = BIT15;
static const int SUBSCRIBED_UPDATE_DELTA_BIT = BIT16;
static const int SUBSCRIBED_DELETE_ACCEPTED_BIT = BIT17;
static const int SUBSCRIBED_DELETE_REJECTED_BIT = BIT18;

static const int SUBSCRIBED_ALL_BITS = SUBSCRIBED_GET_ACCEPTED_BIT | SUBSCRIBED_GET_REJECTED_BIT | SUBSCRIBED_UPDATE_ACCEPTED_BIT | SUBSCRIBED_UPDATE_REJECTED_BIT | SUBSCRIBED_UPDATE_DELTA_BIT | SUBSCRIBED_DELETE_ACCEPTED_BIT | SUBSCRIBED_DELETE_REJECTED_BIT;

struct esp_aws_shadow_handle
{
    esp_mqtt_client_handle_t client;
    esp_event_loop_handle_t event_loop;
    EventGroupHandle_t event_group;
    char topic_prefix[SHADOW_TOPIC_MAX_LENGTH];
    uint8_t topic_prefix_len;

    char thing_name[SHADOW_THINGNAME_LENGTH_MAX];
    char shadow_name[SHADOW_NAME_LENGTH_MAX];

    // For MQTT_EVENT_SUBSCRIBED tracking
    struct topic_substriptions_t
    {
        int get_accepted_msg_id;
        int get_rejected_msg_id;
        int update_accepted_msg_id;
        int update_rejected_msg_id;
        int update_delta_msg_id;
        int delete_accepted_msg_id;
        int delete_rejected_msg_id;
    } topic_substriptions;
};

inline static char *esp_aws_shadow_topic_name(esp_aws_shadow_handle_t handle, const char *topic_suffix, char *topic_buf, uint16_t topic_buf_len)
{
    memcpy(topic_buf, handle->topic_prefix, handle->topic_prefix_len);
    strncpy(topic_buf + handle->topic_prefix_len, topic_suffix, topic_buf_len - handle->topic_prefix_len);
    return topic_buf;
}

static esp_err_t esp_aws_shadow_event_dispatch(esp_event_loop_handle_t event_loop, aws_shadow_event_data_t *event)
{
    esp_err_t err = esp_event_post_to(event_loop, AWS_SHADOW_EVENT, event->event_id, event, sizeof(*event), portMAX_DELAY);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_event_post_to failed: %d", err);
        return err;
    }

    return esp_event_loop_run(event_loop, 0);
}

static esp_err_t esp_aws_shadow_event_dispatch_update_accepted(esp_aws_shadow_handle_t handle, esp_mqtt_event_handle_t event)
{
    aws_shadow_event_data_t shadow_event = AWS_SHADOW_EVENT_DATA_INITIALIZER(handle, AWS_SHADOW_EVENT_UPDATE_ACCEPTED);

    // Parse and publish data (delete after dispatch)
    cJSON *root = esp_aws_shadow_parse_update_accepted(event->data, event->data_len, &shadow_event);
    if (root == NULL)
    {
        ESP_LOGW(TAG, "failed to parse accepted json document");
    }

    esp_err_t err = esp_aws_shadow_event_dispatch(handle->event_loop, &shadow_event);
    cJSON_Delete(root);

    return err;
}

static esp_err_t esp_aws_shadow_event_dispatch_update_delta(esp_aws_shadow_handle_t handle, esp_mqtt_event_handle_t event)
{
    aws_shadow_event_data_t shadow_event = AWS_SHADOW_EVENT_DATA_INITIALIZER(handle, AWS_SHADOW_EVENT_UPDATE_DELTA);

    // Parse and publish data (delete after dispatch)
    cJSON *root = esp_aws_shadow_parse_update_delta(event->data, event->data_len, &shadow_event);
    if (root == NULL)
    {
        ESP_LOGW(TAG, "failed to parse delta json document");
    }

    esp_err_t err = esp_aws_shadow_event_dispatch(handle->event_loop, &shadow_event);
    cJSON_Delete(root);

    return err;
}

static esp_err_t esp_aws_shadow_event_dispatch_error(esp_aws_shadow_handle_t handle, esp_mqtt_event_handle_t event)
{
    aws_shadow_event_data_t shadow_event = AWS_SHADOW_EVENT_DATA_INITIALIZER(handle, AWS_SHADOW_EVENT_ERROR);
    aws_shadow_event_error_t shadow_error = {};

    // TODO provide info, what action actually failed

    // Parse and publish data (delete after dispatch)
    cJSON *root = esp_aws_shadow_parse_error(event->data, event->data_len, &shadow_event, &shadow_error);
    if (root == NULL)
    {
        ESP_LOGW(TAG, "failed to parse error json document");
    }

    esp_err_t err = esp_aws_shadow_event_dispatch(handle->event_loop, &shadow_event);
    cJSON_Delete(root);

    return err;
}

static void esp_aws_shadow_request_get(esp_aws_shadow_handle_t handle)
{
    char topic_name[SHADOW_TOPIC_MAX_LENGTH] = {};
    if (esp_mqtt_client_publish(handle->client, esp_aws_shadow_topic_name(handle, SHADOW_OP_GET, topic_name, sizeof(topic_name)), NULL, 0, 1, 0) == -1)
    {
        ESP_LOGE(TAG, "failed to publish %s" SHADOW_OP_GET, handle->topic_prefix);
    }
}

static void esp_aws_shadow_mqtt_connected(esp_aws_shadow_handle_t handle, esp_mqtt_event_handle_t event)
{
    // Reset tracking
    xEventGroupClearBits(handle->event_group, SUBSCRIBED_ALL_BITS);
    memset(&handle->topic_substriptions, 0, sizeof(handle->topic_substriptions));

    // Subscribe
    char topic_name[SHADOW_TOPIC_MAX_LENGTH] = {};

    handle->topic_substriptions.get_accepted_msg_id = esp_mqtt_client_subscribe(handle->client, esp_aws_shadow_topic_name(handle, (SHADOW_OP_GET SHADOW_SUFFIX_ACCEPTED), topic_name, sizeof(topic_name)), 0);
    handle->topic_substriptions.get_rejected_msg_id = esp_mqtt_client_subscribe(handle->client, esp_aws_shadow_topic_name(handle, (SHADOW_OP_GET SHADOW_SUFFIX_REJECTED), topic_name, sizeof(topic_name)), 0);
    handle->topic_substriptions.update_accepted_msg_id = esp_mqtt_client_subscribe(handle->client, esp_aws_shadow_topic_name(handle, (SHADOW_OP_UPDATE SHADOW_SUFFIX_ACCEPTED), topic_name, sizeof(topic_name)), 0);
    handle->topic_substriptions.update_rejected_msg_id = esp_mqtt_client_subscribe(handle->client, esp_aws_shadow_topic_name(handle, (SHADOW_OP_UPDATE SHADOW_SUFFIX_REJECTED), topic_name, sizeof(topic_name)), 0);
    handle->topic_substriptions.update_delta_msg_id = esp_mqtt_client_subscribe(handle->client, esp_aws_shadow_topic_name(handle, (SHADOW_OP_UPDATE SHADOW_SUFFIX_DELTA), topic_name, sizeof(topic_name)), 0);
    handle->topic_substriptions.delete_accepted_msg_id = esp_mqtt_client_subscribe(handle->client, esp_aws_shadow_topic_name(handle, (SHADOW_OP_DELETE SHADOW_SUFFIX_ACCEPTED), topic_name, sizeof(topic_name)), 0);
    handle->topic_substriptions.delete_rejected_msg_id = esp_mqtt_client_subscribe(handle->client, esp_aws_shadow_topic_name(handle, (SHADOW_OP_DELETE SHADOW_SUFFIX_REJECTED), topic_name, sizeof(topic_name)), 0);

    // Connected state
    xEventGroupSetBits(handle->event_group, CONNECTED_BIT);
    ESP_LOGI(TAG, "%s connected to mqtt server", handle->topic_prefix);
}

static void esp_aws_shadow_mqtt_disconnected(esp_aws_shadow_handle_t handle, esp_mqtt_event_handle_t event)
{
    xEventGroupClearBits(handle->event_group, CONNECTED_BIT | SUBSCRIBED_ALL_BITS);

    aws_shadow_event_data_t shadow_event = AWS_SHADOW_EVENT_DATA_INITIALIZER(handle, AWS_SHADOW_EVENT_DISCONNECTED);
    esp_aws_shadow_event_dispatch(handle->event_loop, &shadow_event);
}

static void esp_aws_shadow_mqtt_subscribed(esp_aws_shadow_handle_t handle, esp_mqtt_event_handle_t event)
{
    EventBits_t bits = 0;

    if (event->msg_id == handle->topic_substriptions.get_accepted_msg_id)
    {
        ESP_LOGD(TAG, "subscribed to %s" SHADOW_OP_GET SHADOW_SUFFIX_ACCEPTED, handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_GET_ACCEPTED_BIT);
    }
    else if (event->msg_id == handle->topic_substriptions.get_rejected_msg_id)
    {
        ESP_LOGD(TAG, "subscribed to %s" SHADOW_OP_GET SHADOW_SUFFIX_REJECTED, handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_GET_REJECTED_BIT);
    }
    else if (event->msg_id == handle->topic_substriptions.update_accepted_msg_id)
    {
        ESP_LOGD(TAG, "subscribed to %s" SHADOW_OP_UPDATE SHADOW_SUFFIX_ACCEPTED, handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_UPDATE_ACCEPTED_BIT);
    }
    else if (event->msg_id == handle->topic_substriptions.update_rejected_msg_id)
    {
        ESP_LOGD(TAG, "subscribed to %s" SHADOW_OP_UPDATE SHADOW_SUFFIX_REJECTED, handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_UPDATE_REJECTED_BIT);
    }
    else if (event->msg_id == handle->topic_substriptions.update_delta_msg_id)
    {
        ESP_LOGD(TAG, "subscribed to %s" SHADOW_OP_UPDATE SHADOW_SUFFIX_DELTA, handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_UPDATE_DELTA_BIT);
    }
    else if (event->msg_id == handle->topic_substriptions.delete_accepted_msg_id)
    {
        ESP_LOGD(TAG, "subscribed to %s" SHADOW_OP_DELETE SHADOW_SUFFIX_ACCEPTED, handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_DELETE_ACCEPTED_BIT);
    }
    else if (event->msg_id == handle->topic_substriptions.delete_rejected_msg_id)
    {
        ESP_LOGD(TAG, "subscribed to %s" SHADOW_OP_DELETE SHADOW_SUFFIX_REJECTED, handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_DELETE_REJECTED_BIT);
    }

    // Ready?
    if ((bits & SUBSCRIBED_ALL_BITS) == SUBSCRIBED_ALL_BITS)
    {
        ESP_LOGI(TAG, "%s is ready", handle->topic_prefix);

        aws_shadow_event_data_t shadow_event = AWS_SHADOW_EVENT_DATA_INITIALIZER(handle, AWS_SHADOW_EVENT_READY);
        esp_aws_shadow_event_dispatch(handle->event_loop, &shadow_event);

        // Request data
        esp_aws_shadow_request_get(handle);
    }
}

static void esp_aws_shadow_mqtt_data_get_op(esp_aws_shadow_handle_t handle, esp_mqtt_event_handle_t event, const char *action, size_t action_len)
{
    const char *op = action + SHADOW_OP_GET_LENGTH;
    size_t op_len = action_len - SHADOW_OP_GET_LENGTH;

    if (op_len == SHADOW_SUFFIX_ACCEPTED_LENGTH && strncmp(op, SHADOW_SUFFIX_ACCEPTED, SHADOW_SUFFIX_ACCEPTED_LENGTH) == 0)
    {
        // /get/accepted
        esp_err_t err = esp_aws_shadow_event_dispatch_update_accepted(handle, event);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "event AWS_SHADOW_EVENT_UPDATE_ACCEPTED dispatch failed: %d", err);
        }
    }
    else if (op_len == SHADOW_SUFFIX_REJECTED_LENGTH && strncmp(op, SHADOW_SUFFIX_REJECTED, SHADOW_SUFFIX_REJECTED_LENGTH) == 0)
    {
        // /get/rejected
        esp_err_t err = esp_aws_shadow_event_dispatch_error(handle, event);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "event AWS_SHADOW_EVENT_ERROR dispatch failed: %d", err);
        }
    }
}

static void esp_aws_shadow_mqtt_data_update_op(esp_aws_shadow_handle_t handle, esp_mqtt_event_handle_t event, const char *action, size_t action_len)
{
    const char *op = action + SHADOW_OP_UPDATE_LENGTH;
    size_t op_len = action_len - SHADOW_OP_UPDATE_LENGTH;

    if (op_len == SHADOW_SUFFIX_ACCEPTED_LENGTH && strncmp(op, SHADOW_SUFFIX_ACCEPTED, SHADOW_SUFFIX_ACCEPTED_LENGTH) == 0)
    {
        // /update/accepted
        esp_err_t err = esp_aws_shadow_event_dispatch_update_accepted(handle, event);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "event AWS_SHADOW_EVENT_UPDATE_ACCEPTED dispatch failed: %d", err);
        }
    }
    else if (op_len == SHADOW_SUFFIX_REJECTED_LENGTH && strncmp(op, SHADOW_SUFFIX_REJECTED, SHADOW_SUFFIX_REJECTED_LENGTH) == 0)
    {
        // /update/rejected
        esp_err_t err = esp_aws_shadow_event_dispatch_error(handle, event);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "event AWS_SHADOW_EVENT_ERROR dispatch failed: %d", err);
        }
    }
    else if (op_len == SHADOW_SUFFIX_DELTA_LENGTH && strncmp(op, SHADOW_SUFFIX_DELTA, SHADOW_SUFFIX_DELTA_LENGTH) == 0)
    {
        // /update/delta
        esp_err_t err = esp_aws_shadow_event_dispatch_update_delta(handle, event);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "event AWS_SHADOW_EVENT_UPDATE_DELTA dispatch failed: %d", err);
        }
    }
}

static void esp_aws_shadow_mqtt_data_delete_op(esp_aws_shadow_handle_t handle, esp_mqtt_event_handle_t event, const char *action, size_t action_len)
{
    const char *op = action + SHADOW_OP_DELETE_LENGTH;
    size_t op_len = action_len - SHADOW_OP_DELETE_LENGTH;

    if (op_len == SHADOW_SUFFIX_ACCEPTED_LENGTH && strncmp(op, SHADOW_SUFFIX_ACCEPTED, SHADOW_SUFFIX_ACCEPTED_LENGTH) == 0)
    {
        // /delete/accepted
        aws_shadow_event_data_t shadow_event = AWS_SHADOW_EVENT_DATA_INITIALIZER(handle, AWS_SHADOW_EVENT_DELETE_ACCEPTED);
        esp_err_t err = esp_aws_shadow_event_dispatch(handle->event_loop, &shadow_event);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "event AWS_SHADOW_EVENT_DELETE_ACCEPTED dispatch failed: %d", err);
        }
    }
    else if (op_len == SHADOW_SUFFIX_REJECTED_LENGTH && strncmp(op, SHADOW_SUFFIX_REJECTED, SHADOW_SUFFIX_REJECTED_LENGTH) == 0)
    {
        // /delete/rejected
        esp_err_t err = esp_aws_shadow_event_dispatch_error(handle, event);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "event AWS_SHADOW_EVENT_ERROR dispatch failed: %d", err);
}
    }
}

static void esp_aws_shadow_mqtt_data(esp_aws_shadow_handle_t handle, esp_mqtt_event_handle_t event)
{
    if (event->topic_len > handle->topic_prefix_len && strncmp(event->topic, handle->topic_prefix, handle->topic_prefix_len) == 0)
    {
        const char *action = event->topic + handle->topic_prefix_len;
        size_t action_len = event->topic_len - handle->topic_prefix_len;

        ESP_LOGI(TAG, "%s action %.*s", handle->topic_prefix, action_len, action);

        if (action_len >= SHADOW_OP_GET_LENGTH && strncmp(action, SHADOW_OP_GET, SHADOW_OP_GET_LENGTH) == 0)
        {
            // Get operation
            esp_aws_shadow_mqtt_data_get_op(handle, event, action, action_len);
        }
        else if (action_len >= SHADOW_OP_UPDATE_LENGTH && strncmp(action, SHADOW_OP_UPDATE, SHADOW_OP_UPDATE_LENGTH) == 0)
        {
            // Update operation
            esp_aws_shadow_mqtt_data_update_op(handle, event, action, action_len);
        }
        else if (action_len >= SHADOW_OP_DELETE_LENGTH && strncmp(action, SHADOW_OP_DELETE, SHADOW_OP_DELETE_LENGTH) == 0)
        {
            // Delete operation
            esp_aws_shadow_mqtt_data_delete_op(handle, event, action, action_len);
        }
    }
}

static void esp_aws_shadow_mqtt_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_aws_shadow_handle_t handle = (esp_aws_shadow_handle_t)handler_args;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        esp_aws_shadow_mqtt_connected(handle, event);
        break;

    case MQTT_EVENT_DISCONNECTED:
        esp_aws_shadow_mqtt_disconnected(handle, event);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        esp_aws_shadow_mqtt_subscribed(handle, event);
        break;

    case MQTT_EVENT_DATA:
        esp_aws_shadow_mqtt_data(handle, event);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGD(TAG, "got mqtt error type: %d", event->error_handle->error_type);
        break;

    default:
        ESP_LOGD(TAG, "unhandled event: %d", event->event_id);
        break;
    }
}

esp_err_t esp_aws_shadow_init(esp_mqtt_client_handle_t client, const char *thing_name, const char *shadow_name, esp_aws_shadow_handle_t *handle)
{
    if (client == NULL || thing_name == NULL || handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    size_t thing_name_len = strlen(thing_name);
    size_t shadow_name_len = shadow_name != NULL ? strlen(shadow_name) : 0;

    if (thing_name_len == 0 || thing_name_len >= SHADOW_THINGNAME_LENGTH_MAX || shadow_name_len >= SHADOW_NAME_LENGTH_MAX)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Alloc
    esp_aws_shadow_handle_t result = (esp_aws_shadow_handle_t)malloc(sizeof(*result));
    if (result == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    // Init
    memset(result, 0, sizeof(*result));

    result->client = client;
    result->event_group = xEventGroupCreate();
    configASSERT(result->event_group);

    esp_event_loop_args_t event_loop_args = {
        .queue_size = 1,
    };
    esp_err_t err = esp_event_loop_create(&event_loop_args, &result->event_loop);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to create event loop: %d", err);
        esp_aws_shadow_delete(result);
        return err;
    }

    strcpy(result->thing_name, thing_name);

    if (shadow_name == NULL)
    {
        // Classic
        snprintf(result->topic_prefix, sizeof(result->topic_prefix), SHADOW_PREFIX_CLASSIC_FORMAT, thing_name);
    }
    else
    {
        // Named
        strcpy(result->shadow_name, shadow_name);
        snprintf(result->topic_prefix, sizeof(result->topic_prefix), SHADOW_PREFIX_NAMED_FORMAT, thing_name, shadow_name);
    }

    result->topic_prefix_len = strlen(result->topic_prefix);
    if (result->topic_prefix == 0)
    {
        ESP_LOGE(TAG, "failed to format topic prefix for '%s' '%s'", thing_name, shadow_name ? shadow_name : "");
        esp_aws_shadow_delete(result);
        return ESP_FAIL;
    }

    // Handler
    err = esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, esp_aws_shadow_mqtt_handler, result);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to register mqtt event handler: %d", err);
        esp_aws_shadow_delete(result);
        return err;
    }

    // Success
    *handle = result;
    return ESP_OK;
}

esp_err_t esp_aws_shadow_delete(esp_aws_shadow_handle_t handle)
{
    if (handle == NULL)
    {
        return ESP_OK;
    }

    // Unregister event handler
    // TODO esp_mqtt_client_unregister_event is not implemented, without it there is a leak, but in real life this won't be used anyway
    // esp_err_t err = esp_mqtt_client_unregister_event(client, MQTT_EVENT_ANY, esp_aws_shadow_mqtt_handler, result);
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

inline esp_err_t esp_aws_shadow_handler_register(esp_aws_shadow_handle_t handle, aws_shadow_event_t event_id, esp_event_handler_t event_handler, void *event_handler_arg)
{
    return esp_aws_shadow_handler_instance_register(handle, event_id, event_handler, event_handler_arg, NULL);
}

inline esp_err_t esp_aws_shadow_handler_instance_register(esp_aws_shadow_handle_t handle, aws_shadow_event_t event_id, esp_event_handler_t event_handler, void *event_handler_arg, esp_event_handler_instance_t *handler_ctx_arg)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_event_handler_instance_register_with(handle->event_loop, AWS_SHADOW_EVENT, event_id, event_handler, event_handler_arg, handler_ctx_arg);
}

inline esp_err_t esp_aws_shadow_handler_instance_unregister(esp_aws_shadow_handle_t handle, aws_shadow_event_t event_id, esp_event_handler_instance_t handler_ctx_arg)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_event_handler_instance_unregister_with(handle->event_loop, AWS_SHADOW_EVENT, event_id, handler_ctx_arg);
}

bool esp_aws_shadow_is_ready(esp_aws_shadow_handle_t handle)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    EventBits_t bits = xEventGroupGetBits(handle->event_group);
    return (bits & SUBSCRIBED_ALL_BITS) == SUBSCRIBED_ALL_BITS;
}

bool esp_aws_shadow_wait_for_ready(esp_aws_shadow_handle_t handle, TickType_t ticks_to_wait)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    EventBits_t bits = xEventGroupWaitBits(handle->event_group, SUBSCRIBED_ALL_BITS, pdFALSE, pdTRUE, ticks_to_wait);
    return (bits & SUBSCRIBED_ALL_BITS) == SUBSCRIBED_ALL_BITS;
}

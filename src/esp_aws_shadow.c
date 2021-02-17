#include "esp_aws_shadow.h"
#include <string.h>
#include <esp_log.h>
#include <freertos/event_groups.h>

static const char TAG[] = "esp_aws_shadow";

/**
 * @brief The maximum length of Thing Name.
 */
#define SHADOW_THINGNAME_LENGTH_MAX (128U)

/**
 * @brief The maximum length of Shadow Name.
 */
#define SHADOW_NAME_LENGTH_MAX (64U)
/**
 * @brief The maximum length of a topic name.
 */
#define SHADOW_TOPIC_MAX_LENGTH (256U)

static const int CONNECTED_BIT = BIT0;
static const int SUBSCRIBED_GET_ACCEPTED_BIT = BIT10;
static const int SUBSCRIBED_GET_REJECTED_BIT = BIT11;
static const int SUBSCRIBED_UPDATE_ACCEPTED_BIT = BIT12;
static const int SUBSCRIBED_UPDATE_REJECTED_BIT = BIT13;
static const int SUBSCRIBED_UPDATE_DELTA_BIT = BIT14;
static const int SUBSCRIBED_UPDATE_DOCUMENTS_BIT = BIT15;

static const int SUBSCRIBED_ALL_BITS = SUBSCRIBED_GET_ACCEPTED_BIT | SUBSCRIBED_GET_REJECTED_BIT | SUBSCRIBED_UPDATE_ACCEPTED_BIT | SUBSCRIBED_UPDATE_REJECTED_BIT | SUBSCRIBED_UPDATE_DELTA_BIT | SUBSCRIBED_UPDATE_DOCUMENTS_BIT;

struct esp_aws_shadow_handle
{
    esp_mqtt_client_handle_t client;
    EventGroupHandle_t event_group;
    char topic_prefix[SHADOW_TOPIC_MAX_LENGTH];
    uint8_t topic_prefix_len;

    char thing_name[SHADOW_THINGNAME_LENGTH_MAX];
    uint8_t thing_name_len;
    char shadow_name[SHADOW_NAME_LENGTH_MAX];
    uint8_t shadow_name_len;

    // For MQTT_EVENT_SUBSCRIBED tracking
    struct topic_substriptions_t
    {
        int get_accepted_msg_id;
        int get_rejected_msg_id;
        int update_accepted_msg_id;
        int update_rejected_msg_id;
        int update_delta_msg_id;
        int update_documents_msg_id;
    } topic_substriptions;
};

inline static char *esp_aws_shadow_topic_name(esp_aws_shadow_handle_t handle, const char *topic_suffix, char *topic_buf, uint16_t topic_buf_len)
{
    memcpy(topic_buf, handle->topic_prefix, handle->topic_prefix_len);
    strncpy(topic_buf + handle->topic_prefix_len, topic_suffix, topic_buf_len - handle->topic_prefix_len);
    return topic_buf;
}

static void esp_aws_shadow_mqtt_connected(esp_mqtt_event_handle_t event, esp_aws_shadow_handle_t handle)
{
    // Reset tracking
    xEventGroupClearBits(handle->event_group, SUBSCRIBED_ALL_BITS);
    memset(&handle->topic_substriptions, 0, sizeof(handle->topic_substriptions));

    // Subscribe
    char topic_name[SHADOW_TOPIC_MAX_LENGTH] = {};

    handle->topic_substriptions.get_accepted_msg_id = esp_mqtt_client_subscribe(handle->client, esp_aws_shadow_topic_name(handle, "/get/accepted", topic_name, sizeof(topic_name)), 0);
    handle->topic_substriptions.get_rejected_msg_id = esp_mqtt_client_subscribe(handle->client, esp_aws_shadow_topic_name(handle, "/get/rejected", topic_name, sizeof(topic_name)), 0);
    handle->topic_substriptions.update_accepted_msg_id = esp_mqtt_client_subscribe(handle->client, esp_aws_shadow_topic_name(handle, "/update/accepted", topic_name, sizeof(topic_name)), 0);
    handle->topic_substriptions.update_rejected_msg_id = esp_mqtt_client_subscribe(handle->client, esp_aws_shadow_topic_name(handle, "/update/rejected", topic_name, sizeof(topic_name)), 0);
    handle->topic_substriptions.update_delta_msg_id = esp_mqtt_client_subscribe(handle->client, esp_aws_shadow_topic_name(handle, "/update/delta", topic_name, sizeof(topic_name)), 0);
    handle->topic_substriptions.update_documents_msg_id = esp_mqtt_client_subscribe(handle->client, esp_aws_shadow_topic_name(handle, "/update/documents", topic_name, sizeof(topic_name)), 0);

    // Connected state
    xEventGroupSetBits(handle->event_group, CONNECTED_BIT);
    ESP_LOGI(TAG, "%s connected to mqtt server", handle->topic_prefix);
}

static void esp_aws_shadow_mqtt_subscribed(esp_mqtt_event_handle_t event, esp_aws_shadow_handle_t handle)
{
    EventBits_t bits = 0;

    if (event->msg_id == handle->topic_substriptions.get_accepted_msg_id)
    {
        ESP_LOGI(TAG, "subscribed to %s/get/accepted", handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_GET_ACCEPTED_BIT);
    }
    else if (event->msg_id == handle->topic_substriptions.get_rejected_msg_id)
    {
        ESP_LOGI(TAG, "subscribed to %s/get/rejected", handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_GET_REJECTED_BIT);
    }
    else if (event->msg_id == handle->topic_substriptions.update_accepted_msg_id)
    {
        ESP_LOGI(TAG, "subscribed to %s/update/accepted", handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_UPDATE_ACCEPTED_BIT);
    }
    else if (event->msg_id == handle->topic_substriptions.update_rejected_msg_id)
    {
        ESP_LOGI(TAG, "subscribed to %s/update/rejected", handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_UPDATE_REJECTED_BIT);
    }
    else if (event->msg_id == handle->topic_substriptions.update_delta_msg_id)
    {
        ESP_LOGI(TAG, "subscribed to %s/update/delta", handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_UPDATE_DELTA_BIT);
    }
    else if (event->msg_id == handle->topic_substriptions.update_documents_msg_id)
    {
        ESP_LOGI(TAG, "subscribed to %s/update/documents", handle->topic_prefix);
        bits = xEventGroupSetBits(handle->event_group, SUBSCRIBED_UPDATE_DOCUMENTS_BIT);
    }

    // For logging
    if ((bits & SUBSCRIBED_ALL_BITS) == SUBSCRIBED_ALL_BITS)
    {
        ESP_LOGI(TAG, "%s is ready", handle->topic_prefix);
    }
}

static void esp_aws_shadow_mqtt_data(esp_mqtt_event_handle_t event, esp_aws_shadow_handle_t handle)
{
    if (event->topic_len > handle->topic_prefix_len && strncmp(event->topic, handle->topic_prefix, handle->topic_prefix_len) == 0)
    {
        const char *action = event->topic + handle->topic_prefix_len;
        size_t action_len = event->topic_len - handle->topic_prefix_len;

        ESP_LOGI(TAG, "%s action %.*s", handle->topic_prefix, action_len, action);
    }
}

static void esp_aws_shadow_mqtt_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_aws_shadow_handle_t handle = (esp_aws_shadow_handle_t)handler_args;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        esp_aws_shadow_mqtt_connected(event, handle);
        break;

    case MQTT_EVENT_DISCONNECTED:
        xEventGroupClearBits(handle->event_group, CONNECTED_BIT | SUBSCRIBED_ALL_BITS);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        esp_aws_shadow_mqtt_subscribed(event, handle);
        break;

    case MQTT_EVENT_DATA:
        esp_aws_shadow_mqtt_data(event, handle);
        break;

        /*
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA, topic=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);

        if (Shadow_MatchTopic(event->topic, event->topic_len, &messageType, &pcThingName, &usThingNameLength) == SHADOW_SUCCESS)
        {
            ESP_LOGI(TAG, "SHADOW thing=%.*s", usThingNameLength, pcThingName);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        // if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        // {
        // 	ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
        // 	ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
        // 	ESP_LOGI(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
        // 			 strerror(event->error_handle->esp_transport_sock_errno));
        // }
        // else
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        }
        else
        {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
        */

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

    if (shadow_name == NULL)
    {
        // Classic
        snprintf(result->topic_prefix, sizeof(result->topic_prefix), "$aws/things/%s/shadow", thing_name);
    }
    else
    {
        // Named
        snprintf(result->topic_prefix, sizeof(result->topic_prefix), "$aws/things/%s/shadow/name/%s", thing_name, shadow_name);
    }
    result->topic_prefix_len = strlen(result->topic_prefix);
    if (result->topic_prefix == 0)
    {
        ESP_LOGE(TAG, "failed to format topic prefix for '%s' '%s'", thing_name, shadow_name ? shadow_name : "");
        esp_aws_shadow_delete(result);
        return ESP_FAIL;
    }

    strcpy(result->thing_name, thing_name);
    result->thing_name_len = thing_name_len;
    if (shadow_name)
    {
        strcpy(result->shadow_name, shadow_name);
        result->shadow_name_len = shadow_name_len;
    }

    // Handler
    esp_err_t err = esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, esp_aws_shadow_mqtt_handler, result);
    if (err != ESP_OK)
    {
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

    // TODO event handler?
    //esp_event_handler_unregister_with(handle->client->)

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

bool esp_aws_shadow_is_ready(esp_aws_shadow_handle_t handle)
{
    EventBits_t bits = xEventGroupGetBits(handle->event_group);
    return (bits & SUBSCRIBED_ALL_BITS) == SUBSCRIBED_ALL_BITS;
}

// TODO
// bool esp_aws_shadow_wait_for_ready(esp_aws_shadow_handle_t handle, uint32_t timeout_ms)
// {
//     EventBits_t bits = xEventGroupWaitBits(handle->event_group, timeout_ms);
//     return (bits & SUBSCRIBED_ALL_BITS) == SUBSCRIBED_ALL_BITS;
// }

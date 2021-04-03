#ifndef AWS_IOT_SHADOW_HANDLE_H
#define AWS_IOT_SHADOW_HANDLE_H

#include "aws_iot_shadow_topic.h"
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <mqtt_client.h>

#ifdef __cplusplus
extern "C" {
#endif

struct topic_subscriptions;

struct aws_iot_shadow_handle
{
    esp_mqtt_client_handle_t client;
    esp_event_loop_handle_t event_loop;
    EventGroupHandle_t event_group;
    char topic_prefix[AWS_IOT_SHADOW_TOPIC_MAX_LENGTH];
    uint8_t topic_prefix_len;

    char thing_name[AWS_IOT_SHADOW_THINGNAME_LENGTH_MAX];
    char shadow_name[AWS_IOT_SHADOW_NAME_LENGTH_MAX];

    struct topic_subscriptions *topic_subscriptions;
};

#ifdef __cplusplus
}
#endif

#endif

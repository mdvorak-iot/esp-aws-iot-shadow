#include "aws_iot_shadow_json.h"
#include <esp_idf_version.h>

// See https://docs.aws.amazon.com/iot/latest/developerguide/device-shadow-document.html#device-shadow-example-response-json

static cJSON *aws_iot_shadow_parse_json(const char *data, size_t data_len)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)
    cJSON *root = cJSON_ParseWithLength(data, data_len);
#else
    // This is unsafe, if invalid json would be received, this might read memory out of bounds
    const char *parse_end = NULL;
    cJSON *root = cJSON_ParseWithOpts(data, &parse_end, 0);
    assert(parse_end <= data + data_len); // At least do sanity check
#endif
    return root;
}

bool aws_iot_shadow_json_is_empty_object(const cJSON *obj)
{
    return !cJSON_IsObject(obj) || obj->child == NULL;
}

static cJSON *aws_iot_shadow_get_item_with_children(cJSON *obj, const char *key)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return item && item->child != NULL ? item : NULL;
}

static uint64_t aws_iot_shadow_get_uint64(cJSON *obj, const char *key)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return item ? (uint64_t)item->valuedouble : 0u;
}

cJSON *aws_iot_shadow_parse_accepted(const char *data, size_t data_len, struct aws_iot_shadow_event_data *output)
{
    cJSON *root = aws_iot_shadow_parse_json(data, data_len);
    cJSON *state = cJSON_GetObjectItemCaseSensitive(root, AWS_IOT_SHADOW_JSON_STATE);
    // Note: these cJSON methods are NULL-safe
    output->root = root;
    output->desired = aws_iot_shadow_get_item_with_children(state, AWS_IOT_SHADOW_JSON_DESIRED);
    output->reported = aws_iot_shadow_get_item_with_children(state, AWS_IOT_SHADOW_JSON_REPORTED);
    output->delta = aws_iot_shadow_get_item_with_children(state, AWS_IOT_SHADOW_JSON_DELTA);
    output->version = aws_iot_shadow_get_uint64(root, AWS_IOT_SHADOW_JSON_VERSION);
    output->client_token = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, AWS_IOT_SHADOW_JSON_CLIENT_TOKEN));

    return root;
}

#if AWS_IOT_SHADOW_SUPPORT_DELTA
cJSON *aws_iot_shadow_parse_update_delta(const char *data, size_t data_len, struct aws_iot_shadow_event_data *output)
{
    cJSON *root = aws_iot_shadow_parse_json(data, data_len);
    // Note: delta document have attributes directly under state attribute
    output->root = root;
    output->delta = aws_iot_shadow_get_item_with_children(root, AWS_IOT_SHADOW_JSON_STATE);
    output->version = aws_iot_shadow_get_uint64(root, AWS_IOT_SHADOW_JSON_VERSION);
    output->client_token = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, AWS_IOT_SHADOW_JSON_CLIENT_TOKEN));
    return root;
}
#endif

cJSON *aws_iot_shadow_parse_error(const char *data, size_t data_len, struct aws_iot_shadow_event_data *output, struct aws_iot_shadow_event_error *error)
{
    cJSON *root = aws_iot_shadow_parse_json(data, data_len);
    cJSON *code = cJSON_GetObjectItemCaseSensitive(root, AWS_IOT_SHADOW_JSON_CODE);

    error->code = code ? code->valueint : 0;
    error->message = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, AWS_IOT_SHADOW_JSON_MESSAGE));

    output->root = root;
    output->client_token = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, AWS_IOT_SHADOW_JSON_CLIENT_TOKEN));
    output->error = error;

    return root;
}

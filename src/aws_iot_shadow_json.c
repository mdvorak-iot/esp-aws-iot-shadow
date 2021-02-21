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

cJSON *aws_iot_shadow_parse_accepted(const char *data, size_t data_len, aws_iot_shadow_event_data_t *output)
{
    cJSON *root = aws_iot_shadow_parse_json(data, data_len);
    cJSON *state = cJSON_GetObjectItemCaseSensitive(root, AWS_IOT_SHADOW_JSON_STATE);
    // Note: these cJSON methods are NULL-safe
    output->root = root;
    output->desired = cJSON_GetObjectItemCaseSensitive(state, AWS_IOT_SHADOW_JSON_DESIRED);
    output->reported = cJSON_GetObjectItemCaseSensitive(state, AWS_IOT_SHADOW_JSON_REPORTED);
    output->delta = cJSON_GetObjectItemCaseSensitive(state, AWS_IOT_SHADOW_JSON_DELTA);
    output->client_token = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, AWS_IOT_SHADOW_JSON_CLIENT_TOKEN));

    return root;
}

cJSON *aws_iot_shadow_parse_update_delta(const char *data, size_t data_len, aws_iot_shadow_event_data_t *output)
{
    cJSON *root = aws_iot_shadow_parse_json(data, data_len);
    // Note: delta document have attributes directly under state attribute
    output->root = root;
    output->delta = cJSON_GetObjectItemCaseSensitive(root, AWS_IOT_SHADOW_JSON_STATE);
    output->client_token = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, AWS_IOT_SHADOW_JSON_CLIENT_TOKEN));
    return root;
}

cJSON *aws_iot_shadow_parse_error(const char *data, size_t data_len, aws_iot_shadow_event_data_t *output, aws_iot_shadow_event_error_t *error)
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

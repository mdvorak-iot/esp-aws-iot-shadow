#include "esp_aws_shadow_json.h"
#include <esp_idf_version.h>

// See https://docs.aws.amazon.com/iot/latest/developerguide/device-shadow-document.html#device-shadow-example-response-json

static cJSON *esp_aws_shadow_parse_json(const char *data, size_t data_len)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)
    cJSON *root = cJSON_ParseWithLength(data, data_len);
#else
    // This is unsafe, if invalid json would be received, this might read memory out of bounds
    const char *parse_end = NULL;
    cJSON *root = cJSON_ParseWithOpts(data, &parse_end, 0);
    configASSERT(parse_end <= data + data_len); // At least do sanity check
#endif
    return root;
}

cJSON *esp_aws_shadow_parse_update_accepted(const char *data, size_t data_len, aws_shadow_event_data_t *output)
{
    cJSON *root = esp_aws_shadow_parse_json(data, data_len);
    cJSON *state = cJSON_GetObjectItemCaseSensitive(root, "state");
    // Note: all cJSON methods are NULL-safe
    output->root = root;
    output->desired = cJSON_GetObjectItemCaseSensitive(state, "desired");
    output->reported = cJSON_GetObjectItemCaseSensitive(state, "reported");
    output->delta = cJSON_GetObjectItemCaseSensitive(state, "delta");
    output->client_token = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "clientToken"));

    return root;
}

cJSON *esp_aws_shadow_parse_update_delta(const char *data, size_t data_len, aws_shadow_event_data_t *output)
{
    cJSON *root = esp_aws_shadow_parse_json(data, data_len);
    // Note: delta document have attributes directly under state attribute
    output->root = root;
    output->delta = cJSON_GetObjectItemCaseSensitive(root, "state");
    output->client_token = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "clientToken"));
    return root;
}

cJSON *esp_aws_shadow_parse_error(const char *data, size_t data_len, aws_shadow_event_data_t *output, aws_shadow_event_error_t *error)
{
    cJSON *root = esp_aws_shadow_parse_json(data, data_len);

    error->code = cJSON_GetObjectItemCaseSensitive(root, "code")->valueint;
    error->message = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "message"));

    output->root = root;
    output->client_token = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "clientToken"));
    output->error = error;

    return root;
}

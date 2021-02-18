#ifndef ESP_AWS_SHADOW_JSON_H_
#define ESP_AWS_SHADOW_JSON_H_

#include "esp_aws_shadow.h"

#ifdef __cplusplus
extern "C"
{
#endif

    cJSON *esp_aws_shadow_parse_update_accepted(const char *data, size_t data_len, aws_shadow_event_data_t *output);

    cJSON *esp_aws_shadow_parse_update_delta(const char *data, size_t data_len, aws_shadow_event_data_t *output);

    cJSON *esp_aws_shadow_parse_error(const char *data, size_t data_len, aws_shadow_event_error_t *output);

#ifdef __cplusplus
}
#endif

#endif

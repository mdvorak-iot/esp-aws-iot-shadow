#ifndef aws_IOT_SHADOW_JSON_H_
#define aws_IOT_SHADOW_JSON_H_

#include "aws_iot_shadow.h"

#ifdef __cplusplus
extern "C" {
#endif

bool aws_iot_shadow_json_is_empty_object(const cJSON *obj);

cJSON *aws_iot_shadow_parse_accepted(const char *data, size_t data_len, struct aws_iot_shadow_event_data *output);

cJSON *aws_iot_shadow_parse_update_delta(const char *data, size_t data_len, struct aws_iot_shadow_event_data *output);

cJSON *aws_iot_shadow_parse_error(const char *data, size_t data_len, struct aws_iot_shadow_event_data *output, struct aws_iot_shadow_event_error *error);

#ifdef __cplusplus
}
#endif

#endif

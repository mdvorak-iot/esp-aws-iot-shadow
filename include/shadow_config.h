#ifndef SHADOW_CONFIG_H_
#define SHADOW_CONFIG_H_

#include <esp_log.h>

#ifndef SHADOW_LOG_TAG
#define SHADOW_LOG_TAG "device_shadow_for_aws"
#endif

/**
 * @brief Macro that is called in the Shadow library for logging "Error" level
 * messages.
 */
#ifndef LogError
#define LogErrorShadowWrapper(format, ...) ESP_LOGE(SHADOW_LOG_TAG, format, ##__VA_ARGS__)
#define LogError(message) LogErrorShadowWrapper message
#endif

/**
 * @brief Macro that is called in the Shadow library for logging "Warning" level
 * messages.
 */
#ifndef LogWarn
#define LogWarnShadowWrapper(format, ...) ESP_LOGW(SHADOW_LOG_TAG, format, ##__VA_ARGS__)
#define LogWarn(message) LogWarnShadowWrapper message
#endif

/**
 * @brief Macro that is called in the Shadow library for logging "Info" level
 * messages.
 */
#ifndef LogInfo
#define LogInfoShadowWrapper(format, ...) ESP_LOGI(SHADOW_LOG_TAG, format, ##__VA_ARGS__)
#define LogInfo(message) LogInfoShadowWrapper message
#endif

/**
 * @brief Macro that is called in the Shadow library for logging "Debug" level
 * messages.
 */
#ifndef LogDebug
#define LogDebugShadowWrapper(format, ...) ESP_LOGD(SHADOW_LOG_TAG, format, ##__VA_ARGS__)
#define LogDebug(message) LogDebugShadowWrapper message
#endif

#endif

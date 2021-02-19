#include "aws_shadow.h"
#include "aws_shadow_mqtt_error.h"
#include <esp_err.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_tls.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <nvs_flash.h>

// TODO
#include "certs.h"

static const char TAG[] = "example";

static bool mqtt_started = false;
static esp_mqtt_client_handle_t mqtt_client = NULL;
static aws_shadow_handle_t shadow_client = NULL;

static void wifi_event_handler(__unused void *handler_args, esp_event_base_t event_base, int32_t event_id, __unused void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        if (!mqtt_started)
        {
            // Initial connection
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mqtt_client_start(mqtt_client));
            mqtt_started = true;
        }
        else
        {
            // Ignore error here
            esp_mqtt_client_reconnect(mqtt_client);
        }
    }
}

static void mqtt_event_handler(__unused void *handler_args, __unused esp_event_base_t event_base, __unused int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id)
    {
    case MQTT_EVENT_DATA:
        // Verbose output for diagnostics, should be omitted in an actual code
        ESP_LOGI(TAG, "topic=%.*s, data=%.*s", event->topic_len, event->topic, event->data_len, event->data);
        break;

    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "connecting to mqtt...");
        break;

    case MQTT_EVENT_ERROR:
        aws_shadow_log_mqtt_error(TAG, event->error_handle);
        break;
    default:
        break;
    }
}

static void shadow_updated(const cJSON *state)
{
    const char *welcome = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(state, "welcome"));
    if (welcome)
    {
        ESP_LOGI(TAG, "got welcome='%s'", welcome);
    }
}

static void shadow_event_handler(__unused void *handler_args, __unused esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    aws_shadow_event_data_t *event = (aws_shadow_event_data_t *)event_data;
    cJSON *version_obj = cJSON_GetObjectItemCaseSensitive(event->root, AWS_SHADOW_JSON_VERSION);
    const char *client_token = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(event->root, AWS_SHADOW_JSON_CLIENT_TOKEN));

    ESP_LOGI(TAG, "received shadow event %d for %s/%s, version %.0f, client_token '%s'", event_id, event->thing_name, event->shadow_name, version_obj ? version_obj->valuedouble : -1, client_token ? client_token : "");

    if (event->event_id == AWS_SHADOW_EVENT_UPDATE_ACCEPTED || event->event_id == AWS_SHADOW_EVENT_UPDATE_DELTA)
    {
        if (event->desired)
        {
            // Handle incoming changes
            shadow_updated(event->desired);
        }
        if (event->delta)
        {
            // Handle incoming changes
            shadow_updated(event->delta);
            // Report they are processed
            // Note: We can reuse delta object, but any extra reported values must be sent independently
            aws_shadow_request_update_reported(event->handle, event->delta, NULL);
        }
    }
    else if (event->event_id == AWS_SHADOW_EVENT_ERROR)
    {
        ESP_LOGW(TAG, "shadow error %d %s", event->error->code, event->error->message);
    }
}

static void setup()
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Events
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

    // NOTE this assumes we have WiFi already stored in NVS

    // MQTT
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.host = CONFIG_AWS_IOT_MQTT_HOST;
    mqtt_cfg.port = CONFIG_AWS_IOT_MQTT_PORT;
    mqtt_cfg.client_id = CONFIG_AWS_IOT_MQTT_CLIENT_ID;
    mqtt_cfg.transport = MQTT_TRANSPORT_OVER_SSL;
    mqtt_cfg.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
    mqtt_cfg.cert_pem = (const char *)AWS_ROOT_CA_PEM;
    mqtt_cfg.client_cert_pem = (const char *)certificate_pem_crt_start;
    mqtt_cfg.client_key_pem = (const char *)private_pem_key_start;

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, NULL));

    // Shadow
    ESP_ERROR_CHECK(aws_shadow_init(mqtt_client, CONFIG_AWS_IOT_THING_NAME, NULL, &shadow_client));
    ESP_ERROR_CHECK(aws_shadow_handler_register(shadow_client, AWS_SHADOW_EVENT_ANY, shadow_event_handler, NULL));

    // Connect
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Setup complete
    ESP_LOGI(TAG, "started");
}

static _Noreturn void run()
{
    // Reuse allocated object every cycle, otherwise it would have to been deleted
    cJSON *reported = cJSON_CreateObject();
    cJSON *now_obj = cJSON_AddNumberToObject(reported, "now", 0);

    for (;;)
    {
        vTaskDelay(30000 / portTICK_PERIOD_MS);

        time_t now;
        time(&now);

        cJSON_SetIntValue(now_obj, now);
        aws_shadow_request_update_reported(shadow_client, reported, NULL);
    }
}

void app_main()
{
    setup();
    run();
}

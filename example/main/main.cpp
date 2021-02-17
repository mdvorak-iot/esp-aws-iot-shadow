#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <mqtt_client.h>
#include <esp_tls.h>
#include <esp_ota_ops.h>
#include <esp_wifi.h>
#include <wifi_reconnect.h>
#include "certs.h"

static const char TAG[] = "example";

static esp_mqtt_client_handle_t mqtt_client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
	esp_mqtt_client_handle_t client = event->client;
	int msg_id;
	// your_context_t *context = event->context;
	switch (event->event_id)
	{
	case MQTT_EVENT_CONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
		// msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
		// ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

		// msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
		// ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

		// msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
		// ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
		break;
	case MQTT_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
		break;

	case MQTT_EVENT_SUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
		msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
		ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
		break;
	case MQTT_EVENT_UNSUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_PUBLISHED:
		ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_DATA:
		ESP_LOGI(TAG, "MQTT_EVENT_DATA");
		printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
		printf("DATA=%.*s\r\n", event->data_len, event->data);
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
	default:
		ESP_LOGI(TAG, "Other event id:%d", event->event_id);
		break;
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

	// Initalize WiFi
	ESP_ERROR_CHECK(esp_netif_init());
	esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
	assert(sta_netif);
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_ERROR_CHECK(wifi_reconnect_start());

	// Connect
	wifi_reconnect_resume();
	if (!wifi_reconnect_wait_for_connection(15000))
	{
		ESP_LOGE(TAG, "wifi connection failed, example cannot continue");
		return;
	}

	// MQTT
	esp_mqtt_client_config_t mqtt_cfg = {};
	mqtt_cfg.host = CONFIG_AWS_IOT_MQTT_HOST;
	mqtt_cfg.port = CONFIG_AWS_IOT_MQTT_PORT;
	mqtt_cfg.client_id = CONFIG_AWS_IOT_MQTT_CLIENT_ID;
	mqtt_cfg.transport = MQTT_TRANSPORT_OVER_SSL;
	mqtt_cfg.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
	mqtt_cfg.cert_pem = (const char *)aws_root_ca_pem_start;
	mqtt_cfg.client_cert_pem = (const char *)certificate_pem_crt_start;
	mqtt_cfg.client_key_pem = (const char *)private_pem_key_start;

	ESP_LOGI(TAG, "free heap: %d bytes", esp_get_free_heap_size());
	mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
	ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));

	// Setup complete
	ESP_LOGI(TAG, "started");
}

static void run()
{
}

extern "C" void app_main()
{
	setup();
	run();
}

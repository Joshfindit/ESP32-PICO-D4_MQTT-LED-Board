#include <string.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <nvs_flash.h>
#include <freertos/event_groups.h>
#include <MQTTClient.h>

// logging constants
#define LOG_TAG "MQTT_LED"

// wifi constants
#define WIFI_CONNECTED_BIT BIT0

// mqtt constants
#define MQTT_HOST                 "192.168.7.20"
#define MQTT_PORT                 1883
#define MQTT_ID                   "ESP32d8a01d4018b4"
#define MQTT_USERNAME             "USERNAME"
#define MQTT_PASSWORD             "PASSWORD"
#define MQTT_TOPIC(relative_name) MQTT_ID "/" relative_name

struct wifi_state_t
{
	EventGroupHandle_t event_group;
};

// out should be of size data->message->payloadlen + 1
void mqtt_get_payload(MessageData *data, char *out)
{
	// copy the payload string limited to its length with a null terminator
	char payload[data->message->payloadlen + 1];
	strncpy(payload, (char *)data->message->payload, data->message->payloadlen);
	payload[data->message->payloadlen] = '\0';

	// copy the payload to  the given output
	strcpy(out, payload);
}

void mqtt_light_handler(MessageData *data)
{
	// get the payload
	char payload[data->message->payloadlen + 1];
	mqtt_get_payload(data, payload);

	// ...
	ESP_LOGI(LOG_TAG, "[APP] Received payload for \"%.*s\": \"%s\"", data->topicName->lenstring.len, data->topicName->lenstring.data, payload);
}

void mqtt_subscribe(MQTTClient *client, const char *topic_name, messageHandler handler)
{
	// create the subscription
	int result = MQTTSubscribe(client, topic_name, QOS0, handler);
	if (result != SUCCESS)
		ESP_LOGE(LOG_TAG, "[APP] Error subscribing to \"%s\": %i", topic_name, result);
	else
		ESP_LOGI(LOG_TAG, "[APP] Subscribed to \"%s\"", topic_name);
}

void mqtt_task()
{
	int result;

	ESP_LOGI(LOG_TAG, "[APP] Starting MQTT...");

	// create the network
	Network network;
	NetworkInit(&network);
	NetworkConnect(&network, MQTT_HOST, MQTT_PORT);

	// create the client
	MQTTClient client;
	unsigned char send_buffer[1000];
	unsigned char read_buffer[1000];

	MQTTClientInit(&client,
                       &network,
                       1000, //command timeout (ms)
                       send_buffer,
                       sizeof(send_buffer),
                       read_buffer,
                       sizeof(read_buffer));

	// create the client id
	MQTTString clientId = MQTTString_initializer;
	clientId.cstring = MQTT_ID;

	// create the connection data
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	data.clientID          = clientId;
	data.willFlag          = 0;
	data.MQTTVersion       = 3;
	data.keepAliveInterval = 0;
	data.cleansession      = 1;
	data.username.cstring  = MQTT_USERNAME;
	data.password.cstring  = MQTT_PASSWORD;

	// connect to the host
	ESP_LOGI(LOG_TAG, "[APP] Connecting to MQTT host...");
	result = MQTTConnect(&client, &data);
	if (result != SUCCESS)
		ESP_LOGE(LOG_TAG, "[APP] Error connecting: %i", result);

	// setup subscriptions
	ESP_LOGI(LOG_TAG, "[APP] Setting up MQTT subscriptions...");
	mqtt_subscribe(&client, MQTT_TOPIC("light"), mqtt_light_handler);

	// run the yield loop
	while(1)
	{
		result = MQTTYield(&client, 1000);
		if (result != SUCCESS)
			ESP_LOGE(LOG_TAG, "[APP] Error yielding: %i", result);
	}

	// delete the task now that the program is finished
	vTaskDelete(NULL);
}

esp_err_t wifi_event_handler(void *context, system_event_t *event)
{
	struct wifi_state_t *state = (struct wifi_state_t *)context;

	switch (event->event_id)
	{
		case SYSTEM_EVENT_STA_START:
			esp_wifi_connect();
			break;
		case SYSTEM_EVENT_STA_GOT_IP:
			xEventGroupSetBits(state->event_group, WIFI_CONNECTED_BIT);
			xTaskCreatePinnedToCore(&mqtt_task, "mqtt_task", 8048, NULL, 5, NULL, 0);
			break;
		case SYSTEM_EVENT_STA_DISCONNECTED:
			esp_wifi_connect();
			xEventGroupClearBits(state->event_group, WIFI_CONNECTED_BIT);
			break;
		default:
			break;
	}

	return ESP_OK;
}

void wifi_init(struct wifi_state_t *state)
{
	ESP_LOGI(LOG_TAG, "[APP] Setting up WiFi...");

	// init tcpip
	tcpip_adapter_init();

	// init event group
	state->event_group = xEventGroupCreate();
	ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, state));

	// init wifi config
	wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

	// init wifi
	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_WIFI_SSID,
			.password = CONFIG_WIFI_PASSWORD,
		},
	};

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ESP_LOGI(LOG_TAG, "[APP] Joining WiFi: SSID:\"%s\"", CONFIG_WIFI_SSID);
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(LOG_TAG, "[APP] Waiting for WiFi...");
	xEventGroupWaitBits(state->event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
}

void app_main()
{
	ESP_LOGI(LOG_TAG, "[APP] Startup...");

	// init logging
	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set(LOG_TAG, ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
	esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

	// init nvs
	nvs_flash_init();

	// init wifi
	struct wifi_state_t state;
	wifi_init(&state);
}

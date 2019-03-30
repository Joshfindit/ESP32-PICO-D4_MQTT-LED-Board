#include <string.h>
#include <assert.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <nvs_flash.h>
#include <freertos/event_groups.h>
#include <MQTTClient.h>
#include <driver/ledc.h>
#include <soc/ledc_struct.h>

// logging constants
#define LOG_TAG "MQTT_LED"

// wifi constants
#define WIFI_CONNECTED_BIT BIT0

// mqtt constants
#define MQTT_MAX_SUBSCRIPTIONS 8

// led constants
#define LED_DUTY_RESOLUTION LEDC_TIMER_10_BIT
#define LED_DUTY_MIN        (0)
#define LED_DUTY_MAX        (1023)
#define LED_MODE            LEDC_HIGH_SPEED_MODE
#define LED_TIMER           LEDC_TIMER_0
#define LED_CHANNEL         LEDC_CHANNEL_0
#define LED_GPIO            (19)

struct wifi_state_t
{
	EventGroupHandle_t event_group;
};

enum led_state_t
{
	LED_STATE_ON,
	LED_STATE_OFF,
};

// pointers to the names of the current mqtt subscriptions
// this is done as MQTTSubscribe needs the passed string to stay in scope during its entire lifetime
// which it cant with the mqtt_subscribe function. so this array stores the mallocd names so they can be freed at the end of the program
unsigned int num_subscription_names = 0;
char *subscription_names[MQTT_MAX_SUBSCRIPTIONS];

// the current led state for this program
enum led_state_t led_state = LED_STATE_OFF;

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

void led_fade_duty(uint32_t duty)
{
	// stop any current fade
	// todo: ledc_get_duty seems to get the target duty of the current fade
	// this breaks the transition as it switches to the final duty of the last fade for half a second before fading
	ledc_stop(LED_MODE, LED_CHANNEL, ledc_get_duty(LED_MODE, LED_CHANNEL));

	// start the fade
	int fade_duration = 300;
	ledc_set_fade_time_and_start(LED_MODE, LED_CHANNEL, duty, fade_duration, LEDC_FADE_NO_WAIT);
}

void led_set_state(enum led_state_t state)
{
	// get the target duty
	uint32_t target_duty = 0;
	switch (state)
	{
		case LED_STATE_ON:
			target_duty = LED_DUTY_MAX;
			break;
		case LED_STATE_OFF:
			target_duty = LED_DUTY_MIN;
			break;
	}

	// fade to the new duty
	led_fade_duty(target_duty);

	// set the new state
	led_state = state;
}

void mqtt_light_switch_handler(MessageData *data)
{
	// get the payload
	char payload[data->message->payloadlen + 1];
	mqtt_get_payload(data, payload);

	// call the appropriate action for the given message
	if (strcmp(payload, "ON") == 0)
		// on
		led_set_state(LED_STATE_ON);
	else if (strcmp(payload, "OFF") == 0)
		// off
		led_set_state(LED_STATE_OFF);
	else if (strcmp(payload, "TOGGLE") == 0)
		// toggle
		led_set_state(!led_state);
}

void mqtt_brightness_handler(MessageData *data)
{
	// get the payload
	char payload[data->message->payloadlen + 1];
	mqtt_get_payload(data, payload);

	// get the given duty
	int duty = atoi(payload);
	if (duty < LED_DUTY_MIN)
		duty = LED_DUTY_MIN;
	else if (duty > LED_DUTY_MAX)
		duty = LED_DUTY_MAX;

	// fade to the new duty
	led_fade_duty(duty);
}

void mqtt_subscribe(MQTTClient *client, const char *client_id, const char *topic_name, messageHandler handler)
{
	// get the full topic
	assert(num_subscription_names <= MQTT_MAX_SUBSCRIPTIONS);
	char *topic = malloc((strlen(client_id) + strlen(topic_name) + 1) * sizeof(char));
	subscription_names[num_subscription_names] = topic;
	num_subscription_names++;

	strcpy(topic, client_id);
	strcat(topic, "/");
	strcat(topic, topic_name);

	// create the subscription
	int result = MQTTSubscribe(client, topic, QOS0, handler);
	if (result != SUCCESS)
		ESP_LOGE(LOG_TAG, "[APP] Error subscribing to \"%s\": %i", topic, result);
	else
		ESP_LOGI(LOG_TAG, "[APP] Subscribed to \"%s\"", topic);
}

void mqtt_task()
{
	int result;

	ESP_LOGI(LOG_TAG, "[APP] Starting MQTT...");

	// create the network
	Network network;
	NetworkInit(&network);
	NetworkConnect(&network, CONFIG_MQTT_SERVER_IP, CONFIG_MQTT_SERVER_PORT);

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

	// get the client id string
	char client_id_string[256];

	// if the mqtt id is set, use that
	if (strlen(CONFIG_MQTT_ID) > 0)
	{
		// set the client id
		strcpy(client_id_string, CONFIG_MQTT_ID);
	}
	// mqtt id is not set, default to "ESP32[mac address]"
	else
	{
		// get the mac address
		uint8_t mac_address[6];
		ESP_ERROR_CHECK(esp_read_mac(mac_address, ESP_MAC_WIFI_STA));

		// convert the mac address to a string
		char mac_address_string[13];
		for (int i = 0; i < 6; i++)
		{
			char part[3];
			sprintf(part, "%02x", mac_address[i]);
			strncpy(&mac_address_string[i * 2], part, 2);
		}
		mac_address_string[12] = '\0';

		// create the full client id
		const char *client_id_prefix = "ESP32";
		strcpy(client_id_string, client_id_prefix);
		strcat(client_id_string, mac_address_string);
	}

	// create the connection data
	MQTTString client_id = MQTTString_initializer;
	client_id.cstring = client_id_string;

	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	data.clientID          = client_id;
	data.willFlag          = 0;
	data.MQTTVersion       = 3;
	data.keepAliveInterval = 0;
	data.cleansession      = 1;
	data.username.cstring  = CONFIG_MQTT_USERNAME;
	data.password.cstring  = CONFIG_MQTT_PASSWORD;

	// connect to the host
	ESP_LOGI(LOG_TAG, "[APP] Connecting to MQTT host...");
	result = MQTTConnect(&client, &data);
	if (result != SUCCESS)
		ESP_LOGE(LOG_TAG, "[APP] Error connecting: %i", result);

	// setup subscriptions
	ESP_LOGI(LOG_TAG, "[APP] Setting up MQTT subscriptions...");
	mqtt_subscribe(&client, client_id_string, "light/switch", mqtt_light_switch_handler);
	mqtt_subscribe(&client, client_id_string, "light/brightness/set", mqtt_brightness_handler);

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

	// led
	// setup the fade timer
	ledc_timer_config_t timer_config =
	{
		.duty_resolution = LED_DUTY_RESOLUTION,
		.freq_hz = 50000,
		.speed_mode = LED_MODE,
		.timer_num = LED_TIMER,
	};

	ledc_timer_config(&timer_config);

	// setup the fade channel
	ledc_channel_config_t channel_config =
	{
		.channel    = LED_CHANNEL,
		.duty       = 0,
		.gpio_num   = LED_GPIO,
		.speed_mode = LED_MODE,
		.hpoint     = 0,
		.timer_sel  = LED_TIMER,
	};

	ledc_channel_config(&channel_config);

	// install the configured fade function
	ledc_fade_func_install(0);

	// free all the subscription names
	for (int i = 0; i < num_subscription_names; i++)
		free(subscription_names[i]);
}

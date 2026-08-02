#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#define WIFI_SSID "Mathias' Sxx U..."
#define WIFI_PASSWORD "1234567890223"

#define MQTT_BROKER_URI "mqtt://102.223.8.140:1883"
#define MQTT_USER "mqtt_user"
#define MQTT_PASS "ega12345"
#define MQTT_TOPIC "honeygrade/iot/moisture"
#define MQTT_STATUS_TOPIC "honeygrade/iot/moisture/status"

#define DEVICE_ID "esp32-moisture-01"

// ESP32 GPIO34 is ADC1 channel 6.
#define MOISTURE_ADC_CHANNEL ADC_CHANNEL_6

// Adjust after checking your exact sensor dry and wet readings.
#define DRY_RAW 3200
#define WET_RAW 1200
#define ADC_FAULT_RAW_MIN 50
#define ADC_SAMPLE_COUNT 21

#define PUBLISH_INTERVAL_MS 500

static const char *TAG = "honeygrade_iot";
static EventGroupHandle_t s_events;
static esp_mqtt_client_handle_t s_mqtt_client;
static adc_oneshot_unit_handle_t s_adc_handle;

#define WIFI_CONNECTED_BIT BIT0
#define MQTT_CONNECTED_BIT BIT1

static float raw_to_moisture_percent(int raw_value)
{
    float percent = ((float)DRY_RAW - (float)raw_value) * 100.0f / ((float)DRY_RAW - (float)WET_RAW);
    if (percent < 0.0f) {
        percent = 0.0f;
    }
    if (percent > 100.0f) {
        percent = 100.0f;
    }
    return percent;
}

static int compare_int(const void *a, const void *b)
{
    int left = *(const int *)a;
    int right = *(const int *)b;
    return (left > right) - (left < right);
}

static bool read_stable_raw(int *raw_value)
{
    int samples[ADC_SAMPLE_COUNT] = {0};

    for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(s_adc_handle, MOISTURE_ADC_CHANNEL, &samples[i]));
        vTaskDelay(pdMS_TO_TICKS(8));
    }

    qsort(samples, ADC_SAMPLE_COUNT, sizeof(samples[0]), compare_int);

    int start = ADC_SAMPLE_COUNT / 4;
    int end = ADC_SAMPLE_COUNT - start;
    int sum = 0;
    int count = 0;
    for (int i = start; i < end; i++) {
        sum += samples[i];
        count++;
    }

    *raw_value = count > 0 ? sum / count : samples[ADC_SAMPLE_COUNT / 2];
    return *raw_value > ADC_FAULT_RAW_MIN;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi starting, connecting to %s", WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT);
        ESP_LOGW(TAG, "WiFi disconnected, reason=%d. Reconnecting...", event->reason);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "WiFi connected. IP=" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        xEventGroupSetBits(s_events, MQTT_CONNECTED_BIT);
        ESP_LOGI(TAG, "MQTT connected");
        esp_mqtt_client_publish(s_mqtt_client, MQTT_STATUS_TOPIC, "online", 0, 1, 1);
        break;
    case MQTT_EVENT_DISCONNECTED:
        xEventGroupClearBits(s_events, MQTT_CONNECTED_BIT);
        ESP_LOGW(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT error");
        break;
    default:
        break;
    }

    (void)handler_args;
    (void)base;
    (void)event;
}

static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.client_id = DEVICE_ID,
        .credentials.username = MQTT_USER,
        .credentials.authentication.password = MQTT_PASS,
        .session.last_will.topic = MQTT_STATUS_TOPIC,
        .session.last_will.msg = "offline",
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_mqtt_client));
}

static void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &s_adc_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, MOISTURE_ADC_CHANNEL, &config));
}

static void publish_task(void *arg)
{
    char payload[256];

    while (true) {
        EventBits_t bits = xEventGroupWaitBits(
            s_events,
            WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT,
            pdFALSE,
            pdTRUE,
            pdMS_TO_TICKS(10000)
        );

        if ((bits & (WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT)) != (WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT)) {
            ESP_LOGW(TAG, "Waiting for WiFi and MQTT...");
            continue;
        }

        int raw_value = 0;
        int rssi = 0;
        if (!read_stable_raw(&raw_value)) {
            ESP_LOGW(TAG, "Skipping invalid moisture ADC reading raw=%d. Check sensor VCC/GND/AO and GPIO34.", raw_value);
            vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL_MS));
            continue;
        }
        esp_wifi_sta_get_rssi(&rssi);
        float moisture_percent = raw_to_moisture_percent(raw_value);

        snprintf(
            payload,
            sizeof(payload),
            "{\"device_id\":\"%s\",\"raw_value\":%d,\"moisture_percent\":%.2f,\"wifi_rssi\":%d,\"uptime_ms\":%lu}",
            DEVICE_ID,
            raw_value,
            moisture_percent,
            rssi,
            (unsigned long)(esp_timer_get_time() / 1000)
        );

        int msg_id = esp_mqtt_client_publish(s_mqtt_client, MQTT_TOPIC, payload, 0, 0, 0);
        ESP_LOGI(TAG, "Published msg_id=%d %s", msg_id, payload);

        vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL_MS));
    }

    (void)arg;
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_events = xEventGroupCreate();
    adc_init();
    wifi_init();
    mqtt_init();

    xTaskCreate(publish_task, "publish_moisture", 4096, NULL, 5, NULL);
}

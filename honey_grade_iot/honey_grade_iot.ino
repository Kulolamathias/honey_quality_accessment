#include <WiFi.h>
#include <PubSubClient.h>

#define WIFI_SSID "Mathias' Sxx U..."
#define WIFI_PASSWORD "1234567890223"

#define MQTT_HOST "102.223.8.140"
#define MQTT_PORT 1883
#define MQTT_USER "mqtt_user"
#define MQTT_PASS "ega12345"
#define MQTT_TOPIC "honeygrade/iot/moisture"
#define MQTT_STATUS_TOPIC "honeygrade/iot/moisture/status"

#define DEVICE_ID "esp32-moisture-01"
#define MOISTURE_PIN 34

// Adjust these two values after testing your exact sensor in air and in wet honey/water.
#define DRY_RAW 3200
#define WET_RAW 1200
#define ADC_FAULT_RAW_MIN 50
#define ADC_SAMPLE_COUNT 21

#define PUBLISH_INTERVAL_MS 500

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastPublishMs = 0;
unsigned long lastWifiRetryMs = 0;

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println();
      Serial.println("WiFi event: connected to access point");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("WiFi event: got IP ");
      Serial.println(WiFi.localIP());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println();
      Serial.print("WiFi event: disconnected, reason ");
      Serial.println(info.wifi_sta_disconnected.reason);
      break;
    default:
      break;
  }
}

float rawToMoisturePercent(int rawValue) {
  float percent = ((float)DRY_RAW - (float)rawValue) * 100.0 / ((float)DRY_RAW - (float)WET_RAW);
  if (percent < 0.0) percent = 0.0;
  if (percent > 100.0) percent = 100.0;
  return percent;
}

bool readStableRaw(int &rawValue) {
  int samples[ADC_SAMPLE_COUNT];

  for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
    samples[i] = analogRead(MOISTURE_PIN);
    delay(8);
  }

  for (int i = 1; i < ADC_SAMPLE_COUNT; i++) {
    int key = samples[i];
    int j = i - 1;
    while (j >= 0 && samples[j] > key) {
      samples[j + 1] = samples[j];
      j--;
    }
    samples[j + 1] = key;
  }

  int start = ADC_SAMPLE_COUNT / 4;
  int end = ADC_SAMPLE_COUNT - start;
  long sum = 0;
  int count = 0;
  for (int i = start; i < end; i++) {
    sum += samples[i];
    count++;
  }

  rawValue = count > 0 ? sum / count : samples[ADC_SAMPLE_COUNT / 2];
  return rawValue > ADC_FAULT_RAW_MIN;
}

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("WiFi connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.setHostname(DEVICE_ID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    unsigned long now = millis();
    if (now - lastWifiRetryMs >= 15000) {
      lastWifiRetryMs = now;
      Serial.println();
      Serial.print("Still waiting for WiFi, status ");
      Serial.println(WiFi.status());
      WiFi.disconnect(false);
      delay(300);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      Serial.print("WiFi retrying ");
      Serial.println(WIFI_SSID);
    }
  }

  Serial.println();
  Serial.print("WiFi connected. IP: ");
  Serial.println(WiFi.localIP());
}

void connectMqtt() {
  if (WiFi.status() != WL_CONNECTED) return;

  while (!mqttClient.connected()) {
    Serial.print("MQTT connecting...");
    if (mqttClient.connect(DEVICE_ID, MQTT_USER, MQTT_PASS, MQTT_STATUS_TOPIC, 1, true, "offline")) {
      Serial.println("connected");
      mqttClient.publish(MQTT_STATUS_TOPIC, "online", true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 3 seconds");
      delay(3000);
    }
  }
}

void publishMoisture() {
  int rawValue = 0;
  if (!readStableRaw(rawValue)) {
    Serial.print("Skipping invalid moisture ADC reading raw=");
    Serial.println(rawValue);
    Serial.println("Check sensor VCC/GND/AO and GPIO34 wiring.");
    return;
  }
  float moisturePercent = rawToMoisturePercent(rawValue);

  char payload[220];
  snprintf(
    payload,
    sizeof(payload),
    "{\"device_id\":\"%s\",\"raw_value\":%d,\"moisture_percent\":%.2f,\"wifi_rssi\":%d,\"uptime_ms\":%lu}",
    DEVICE_ID,
    rawValue,
    moisturePercent,
    WiFi.RSSI(),
    millis()
  );

  bool ok = mqttClient.publish(MQTT_TOPIC, payload);
  Serial.print(ok ? "Published: " : "Publish failed: ");
  Serial.println(payload);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  WiFi.onEvent(onWiFiEvent);

  analogReadResolution(12);
  analogSetPinAttenuation(MOISTURE_PIN, ADC_11db);

  connectWifi();
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  connectMqtt();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWifi();
  }

  if (!mqttClient.connected()) {
    connectMqtt();
  }

  if (WiFi.status() != WL_CONNECTED || !mqttClient.connected()) {
    return;
  }

  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastPublishMs >= PUBLISH_INTERVAL_MS) {
    lastPublishMs = now;
    publishMoisture();
  }
}

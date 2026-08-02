# HoneyGrade ESP-IDF Moisture MQTT

This is the recommended ESP32 firmware if the Arduino Wi-Fi stack is unreliable with your phone hotspot.

It publishes moisture readings every 500 ms to:

```text
honeygrade/iot/moisture
```

## Open In VS Code ESP-IDF

Open this folder:

```text
H:\engRel\ai_assisted\honey_quality_assessment\honey_grade_iot\espidf_moisture_mqtt
```

Then use the ESP-IDF extension:

```text
Set Espressif Device Target: esp32
Build
Flash
Monitor
```

## Configure Wi-Fi

Edit `main/main.c`:

```c
#define WIFI_SSID "Mathias' Sxx U..."
#define WIFI_PASSWORD "1234567890223"
```

Use the exact full hotspot name. If your phone only shows a shortened name in UI, check the full hotspot name in Android hotspot settings.

## MQTT

The broker is already configured:

```c
#define MQTT_BROKER_URI "mqtt://102.223.8.140:1883"
#define MQTT_USER "mqtt_user"
#define MQTT_PASS "ega12345"
```

## Sensor Pin

GPIO34 is used:

```c
#define MOISTURE_ADC_CHANNEL ADC_CHANNEL_6
```

Wiring:

```text
Sensor AO/SIG -> ESP32 GPIO34
Sensor VCC    -> ESP32 3.3V
Sensor GND    -> ESP32 GND
```

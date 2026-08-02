# HoneyGrade ESP32 Moisture MQTT

This Arduino IDE sketch reads one analog moisture sensor on ESP32 GPIO 34 and publishes JSON readings to the HoneyGrade MQTT topic:

```text
honeygrade/iot/moisture
```

## Arduino IDE Libraries

Install these from Arduino IDE Library Manager:

```text
PubSubClient
```

The ESP32 board package must also be installed in Arduino IDE.

## Wiring

```text
Sensor AO/SIG -> ESP32 GPIO 34
Sensor VCC    -> ESP32 3.3V
Sensor GND    -> ESP32 GND
```

## Configure

Open `honey_grade_iot.ino` and set:

```cpp
#define WIFI_SSID "PUT_YOUR_WIFI_NAME_HERE"
#define WIFI_PASSWORD "1234567890223"
```

The MQTT broker is already configured as:

```cpp
#define MQTT_HOST "102.223.8.140"
#define MQTT_PORT 1883
#define MQTT_USER "mqtt_user"
#define MQTT_PASS "ega12345"
```

## Payload

The ESP32 publishes JSON like:

```json
{
  "device_id": "esp32-moisture-01",
  "raw_value": 2410,
  "moisture_percent": 39.50,
  "wifi_rssi": -61,
  "uptime_ms": 15000
}
```

## Calibration

Start with:

```cpp
#define DRY_RAW 3200
#define WET_RAW 1200
```

Read the Serial Monitor while the sensor is dry and while it is in your wet test sample, then update those values. The first integration only displays moisture. Honey quality grading rules will be added later after choosing proper research-backed ranges.

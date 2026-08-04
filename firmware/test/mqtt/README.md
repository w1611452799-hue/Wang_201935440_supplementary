# mqtt Test

ESP32-S3 MQTT over TLS test project. The device connects to the EMQX global public test broker (`broker.emqx.io`) through WiFi. It publishes simulated temperature, humidity, and pressure data every 10 seconds. It also subscribes to a command topic and prints received messages.

---

## Configuration

Modify the macro definitions at the top of `main/main.c`:

```c
#define WIFI_SSID        "your_ssid"      // WiFi name
#define WIFI_PASSWD      "your_password"  // WiFi password
#define WIFI_MAX_RETRY   5                // Maximum number of retries
```

> The MQTT broker, topics, and certificate are already configured and do not need to be modified.

---

## MQTT Connection Information

Public service used: https://www.emqx.com/en/mqtt/public-mqtt5-broker

| Item              | Value                                                |
| ----------------- | ---------------------------------------------------- |
| Broker            | `broker.emqx.io`                                     |
| Port              | `8883` (MQTTS / TLS)                                 |
| Authentication    | No username or password; CA certificate verification |
| Publish topic     | `fr_plant_water/esp32/sensors`                       |
| Subscribe topic   | `fr_plant_water/esp32/cmd`                           |
| Publish frequency | Once every 10 seconds                                |

> The topics use `fr_plant_water/` as the prefix to avoid conflicts with public broadcast messages.

---

## Message Format

### Publish Message (Sensor Data)

Topic: `fr_plant_water/esp32/sensors`

```json
{
    "temperature": 24.53,
    "humidity": 62.18,
    "pressure": 1013.25
}
```

| Field         | Unit | Range    |
| ------------- | ---- | -------- |
| `temperature` | °C   | 10–35    |
| `humidity`    | %    | 30–90    |
| `pressure`    | hPa  | 950–1050 |

### Subscribe Message (Command Topic)

Topic: `fr_plant_water/esp32/cmd`

Any received content will be printed through the serial port and can be used to verify the reception of downlink commands.

---

## Build and Flash

```bash
cd software/test/mqtt
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

---

## Expected Serial Output

```text
I (xxx) mqtt_test: WiFi STA started, connecting...
I (xxx) mqtt_test: Got IP: 192.168.1.xxx
I (xxx) mqtt_test: MQTT connected
I (xxx) mqtt_test: Subscribed to fr_plant_water/esp32/cmd
I (xxx) mqtt_test: Simulated: temp=23.45 humi=58.12 pres=1008.37
I (xxx) mqtt_test: Publish -> fr_plant_water/esp32/sensors : {"temperature":23.45,...}
I (xxx) mqtt_test: Publish ack, msg_id=xxx
```

---

## Online Debugging

You can use [MQTTX](https://mqttx.app/) or any MQTT client to connect to `broker.emqx.io:8883`. TLS must be enabled and the CA certificate must be loaded. Subscribe to `fr_plant_water/esp32/sensors` to view the data published by the device in real time.

Publish any message to `fr_plant_water/esp32/cmd` to verify that the device can receive downlink messages.

---

## Required Components

| Component   | Description                         |
| ----------- | ----------------------------------- |
| `esp_wifi`  | WiFi driver                         |
| `esp_event` | Event loop                          |
| `esp_netif` | Network interface abstraction layer |
| `nvs_flash` | NVS initialization                  |
| `mqtt`      | ESP-MQTT client                     |
| `cJSON`     | JSON serialization                  |

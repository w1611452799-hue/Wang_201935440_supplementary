# wifi_sta Test

ESP32-S3 WiFi STA mode connection test project. It verifies whether the device can successfully connect to a specified AP and obtain an IP address, with support for automatic reconnection after disconnection.

---

## Features

* Connects to a specified WiFi AP in Station mode
* Automatically retries after a failed connection, up to `ESP_WIFI_MAX_RETRY` times, with a default value of 5
* Prints the IP address through the serial port after obtaining an IP address
* Resets the retry counter after successfully obtaining an IP address, allowing automatic reconnection after a disconnection
* Disables power-saving mode (`WIFI_PS_NONE`) to ensure fast data response

---

## Configuration

Modify the macro definitions at the top of `main/main.c`:

```c
#define ESP_WIFI_STA_SSID    "your_ssid"       // WiFi name
#define ESP_WIFI_STA_PASSWD  "your_password"   // WiFi password
#define ESP_WIFI_MAX_RETRY   5                 // Maximum number of retries
```

---

## Build and Flash

```bash
cd software/test/wifi_sta
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

---

## Expected Serial Output

**Successful connection:**

```text
I (xxx) wifi_sta: STA started, connecting...
I (xxx) wifi_sta: Got IP: 192.168.1.xxx
```

**Connection failure and retry:**

```text
I (xxx) wifi_sta: STA started, connecting...
W (xxx) wifi_sta: Disconnected, retry 1/5...
W (xxx) wifi_sta: Disconnected, retry 2/5...
...
E (xxx) wifi_sta: Failed to connect after 5 retries, give up.
```

---

## Required Components

| Component   | Description                                   |
| ----------- | --------------------------------------------- |
| `esp_wifi`  | WiFi driver                                   |
| `esp_event` | Event loop                                    |
| `esp_netif` | Network interface abstraction layer           |
| `nvs_flash` | NVS for persistent WiFi configuration storage |

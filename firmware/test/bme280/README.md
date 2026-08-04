# BME280 Test Project (ESP32-S3)

This project tests the BME280 temperature, humidity, and pressure sensor on the ESP32-S3.
The code is fully decoupled from the legacy `i2c_bus` component and directly uses the native `i2c_master` API from **ESP-IDF v5.x**, making it easy to share the I2C bus with other devices (e.g., MLX90640).

## Hardware Requirements
- ESP32-S3 development board
- BME280 sensor module
- Connecting wires

## Wiring
I2C pin configuration (can be modified in `main/main.c`). The BME280 uses a dedicated I2C bus:
- **SDA**: GPIO 22
- **SCL**: GPIO 21
- **VCC**: 3.3V
- **GND**: GND

*Note: The default I2C address is `0x76`. If your module uses `0x77`, modify it in the initialization code or header file.*

## Software Requirements
- [ESP-IDF v5.x](https://github.com/espressif/esp-idf)

## Build & Run
Uses the standard ESP-IDF v5 CMake build system:

1. Set up the ESP-IDF environment:
   ```bash
   . $HOME/esp/esp-idf/export.sh
   # Or on Windows, run export.bat
   ```
2. Build, flash, and monitor:
   ```bash
   idf.py build
   idf.py -p (your_serial_port) flash monitor
   ```

## Program Description
- Entry point is `main/main.c`
- In the `main` function, the global I2C bus is initialized and the BME280 device is registered.
- The system reads temperature, humidity, and pressure data from the sensor every 1 second and outputs them via serial log.
- `components/bme280` is a pure sensor driver component. It does not include any bus creation logic; it only receives an `i2c_master_dev_handle_t` handle for data communication.

## Sample Output

After a successful test, the serial output looks like:

```text
I (275) main_task: Started on CPU0
I (285) main_task: Calling app_main()
I (285) main: I2C bus initialized successfully
I (1605) main: Temperature: 25.11 °C, Humidity: 27.42 %, Pressure: 803.27 hPa
I (2605) main: Temperature: 25.12 °C, Humidity: 27.41 %, Pressure: 803.23 hPa
I (3605) main: Temperature: 25.13 °C, Humidity: 27.50 %, Pressure: 803.34 hPa
I (4605) main: Temperature: 25.09 °C, Humidity: 27.48 %, Pressure: 803.17 hPa
I (5605) main: Temperature: 25.04 °C, Humidity: 27.52 %, Pressure: 803.17 hPa
I (6605) main: Temperature: 24.47 °C, Humidity: 27.52 %, Pressure: 802.39 hPa
I (7605) main: Temperature: 25.11 °C, Humidity: 27.41 %, Pressure: 802.92 hPa
I (8605) main: Temperature: 24.85 °C, Humidity: 27.33 %, Pressure: 802.98 hPa
I (9605) main: Temperature: 24.99 °C, Humidity: 27.31 %, Pressure: 803.20 hPa
I (10605) main: Temperature: 25.09 °C, Humidity: 27.31 %, Pressure: 803.26 hPa
```

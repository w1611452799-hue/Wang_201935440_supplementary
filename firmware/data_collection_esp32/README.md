# Data Collection Firmware (ESP32-S3)

This firmware reads PT100 temperature via MAX31865 (SPI) and ambient temperature, humidity, and pressure via BME280 (I2C), then outputs CSV-formatted data over the default serial port (UART0 / USB-CDC).

## Hardware Requirements

- ESP32-S3 development board
- MAX31865 module with PT100 RTD (3-wire)
- BME280 sensor module
- Connecting wires

## Wiring

### BME280 (I2C)
| Signal | GPIO |
|--------|------|
| SDA    | 22   |
| SCL    | 21   |
| VCC    | 3.3V |
| GND    | GND  |

### MAX31865 (SPI)
| Signal | GPIO |
|--------|------|
| MOSI   | 23   |
| MISO   | 19   |
| SCLK   | 18   |
| CS     | 5    |
| VCC    | 3.3V |
| GND    | GND  |

*Note: The default I2C address for BME280 is `0x76`. If your module uses `0x77`, change `BME280_I2C_ADDRESS_DEFAULT` in `components/bme280/include/bme280.h`.*

## Serial Output Format

The firmware prints a CSV header on startup, followed by data lines at ~1 Hz:

```
PT100_C,BME280_T_C,Humidity_%,Pressure_hPa
25.12,25.11,27.42,803.27
25.14,25.13,27.41,803.25
...
```

- Column 1: PT100 temperature in °C (via MAX31865)
- Column 2: BME280 ambient temperature in °C
- Column 3: Relative humidity in %
- Column 4: Atmospheric pressure in hPa

A value of `-999.00` in any column indicates a sensor read failure.

## Build & Flash

Requires [ESP-IDF v5.x](https://github.com/espressif/esp-idf).

```bash
# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Build, flash, and monitor
idf.py build
idf.py -p <serial_port> flash monitor
```

## Configuration

Key parameters can be modified in `main/main.c`:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `I2C_MASTER_SDA_IO` | 22 | BME280 SDA pin |
| `I2C_MASTER_SCL_IO` | 21 | BME280 SCL pin |
| `PIN_MOSI` | 23 | MAX31865 MOSI pin |
| `PIN_MISO` | 19 | MAX31865 MISO pin |
| `PIN_SCLK` | 18 | MAX31865 SCLK pin |
| `PIN_CS` | 5 | MAX31865 CS pin |
| `RTD_NOMINAL` | 100.0 | PT100 nominal resistance |
| `RTD_REF` | 430.0 | Reference resistor on MAX31865 board |

## LCD Display (Optional)

An ST7789 135×240 LCD can be connected for real-time sensor data
visualization.  The display is **optional** — data collection works
normally via serial whether or not a screen is attached.

### Wiring

| Signal | GPIO (ESP32) | GPIO (ESP32-S3) | LCD Pin |
|--------|-------------|-----------------|---------|
| SCLK   | 5           | 12              | SCL     |
| MOSI   | 16          | 11              | SDA     |
| DC     | 17          | 10              | DC      |
| CS     | 4           | 13              | CS      |
| RST    | 2           | 14              | RST     |
| VCC    | 3.3V        | 3.3V            | VCC     |
| GND    | GND         | GND             | GND     |
| BL     | 3.3V        | 3.3V            | LED     |

*Note: The LCD uses a separate SPI bus from the MAX31865 — no pin conflicts.*

### Enabling / Disabling

Run `idf.py menuconfig`, navigate to
`Data Collection - LCD Display (ST7789)`, and toggle
`Enable ST7789 LCD display`.

When enabled without a screen connected, the init sequence fails
gracefully and the firmware continues with serial output only.

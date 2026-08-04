# ST7789 LCD Display Example

ESP32-driven ST7789 240×240 LCD screen with an LVGL v9 dashboard UI.

## Hardware Configuration

* **Driver IC**: ST7789
* **Resolution**: 240×240
* **Interface**: 4-wire SPI (SCL / SDA / DC / CS) + RST hardware reset
* **Development board**: ESP32 DevKit V1 (classic 30-pin version)

## Pin Connections

| Function | GPIO   | Board Label | Screen Label | Notes                                                   |
| :------- | :----- | :---------- | :----------- | :------------------------------------------------------ |
| SPI SCLK | **5**  | D5          | SCL / CLK    | SPI clock signal                                        |
| SPI MOSI | **16** | RX2         | SDA / DIN    | Host output → screen input                              |
| LCD DC   | **17** | TX2         | DC / RS / A0 | Data/command selection                                  |
| LCD CS   | **4**  | D4          | CS           | Chip select, automatically controlled by the SPI driver |
| LCD RST  | **2**  | D2          | RST / RES    | Hardware reset                                          |
| SPI MISO | —      | —           | —            | Not connected, one-way transmission only                |

```text
Board D5  → Screen SCL
Board RX2 → Screen SDA
Board TX2 → Screen DC
Board D4  → Screen CS
Board D2  → Screen RST
```

## Software Description

* **ESP-IDF**: v5.5.3
* **GUI**: LVGL v9.2.0 — environmental monitoring dashboard (temperature / humidity / pressure)
* Uses the `esp_lcd` driver framework, with CS automatically managed by the driver
* Demonstrates simulated sensor data, refreshed once per second

## Build and Run

```bash
cd 04_implementation/firmware/test/st7789_lcd_display
idf.py build flash monitor
```


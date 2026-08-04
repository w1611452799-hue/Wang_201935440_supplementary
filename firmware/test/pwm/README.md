# ESP32 PWM Control Project (LEDC & UART0)

This project implements PWM output based on the ESP32 **LEDC** peripheral and supports real-time duty cycle control through **Serial Port 0 (UART0)**. It is mainly used to control a MOSFET-driven heating element or other power loads.

## 1. Hardware Connections

* **PWM output pin**: **GPIO 2** is used by default. This can be changed in `main.c` by modifying `LEDC_OUTPUT_IO`.
* **Serial connection**: Uses the ESP32 default Serial Port 0 (TX: GPIO 1, RX: GPIO 3).

## 2. Serial Control Instructions

* **Baud rate**: 115200
* **Data bits**: 8 bits
* **Stop bits**: 1 bit
* **Parity**: None

### Control Method

Use a serial debugging tool, such as `screen`, `minicom`, `putty`, or a VS Code extension, to send an integer between **0 and 100**. This adjusts the PWM duty cycle.

* **Send `0`**: The PWM duty cycle is set to 0% and the output is disabled.
* **Send `50`**: The PWM duty cycle is set to 50%.
* **Send `100`**: The PWM duty cycle is set to 100% and runs at full output.

### Response Message

After a command is sent, the serial port displays the currently configured duty cycle percentage. For example:

`PWM duty cycle set to: 50%`

## 3. Technical Specifications

* **PWM frequency**: 5 kHz, suitable for most MOSFET drivers.
* **PWM resolution**: 13 bits (0–8191), providing smooth power adjustment.
* **LEDC mode**: Low Speed Mode.

## 4. Build and Run

1. Enter the project directory:

   ```bash
   cd firmware/demo/pwm
   ```

2. Build, flash, and open the serial monitor:

   ```bash
   idf.py build flash monitor
   ```

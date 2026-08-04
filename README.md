This directory stores implementation-facing materials copied out from the backup/reference area and cited by the report appendix.

simulation/simulink/: Simulink model files and modeling records.
simulation/LTspice/: LTspice circuit files and MOSFET library.
firmware/data_collection_esp32/: complete ESP32 data acquisition project.
firmware/thermal_pid_control/: complete ESP32 heater control project.
firmware/data_collection_pc/: complete PC data collection application.

# Implementation

This directory contains the main implementation materials for an ESP32-based platform used to evaluate the environmental robustness of fabric thermal performance. The platform combines controlled heating, temperature and environmental sensing, wind-tunnel testing, data communication, and PC-side data recording.

The system was developed to compare how different fabrics respond to dry, wet, and windy conditions. A heated metal cylinder is used as a repeatable thermal source. Its temperature is measured by a PT100 sensor through a MAX31865 interface, while an ESP32 controls the heater through PWM and a MOSFET power stage. Environmental measurements and test data can be collected, transmitted, displayed, and saved for later analysis.

## Directory Structure

```text
implementation/
├── simulation/
│   ├── simulink/
│   │   ├── PWM electrothermal models
│   │   ├── thermal RC models
│   │   └── modelling records
│   └── LTspice/
│       ├── heater and MOSFET circuit files
│       └── MOSFET device library
│
└── firmware/
    ├── data_collection_esp32/
    │   └── ESP32 sensor acquisition and communication project
    ├── thermal_pid_control/
    │   └── ESP32 closed-loop heater control project
    └── data_collection_pc/
        └── PC-side data collection and logging application
```

## Project Overview

The implementation is divided into three main parts:

1. **Simulation and modelling**  
   Simulink and LTspice are used to study the electrical and thermal behaviour of the heating system before hardware testing.

2. **Embedded control and data acquisition**  
   The ESP32 reads temperature and environmental sensors, controls the heater, manages safety limits, and publishes or records experimental data.

3. **PC-side data collection**  
   The PC application receives, displays, and stores the data generated during wind-tunnel and fabric tests.

## Main System Functions

- PT100 temperature measurement through the MAX31865
- PWM heater control through the ESP32 LEDC peripheral
- MOSFET-based switching of the heating element
- Closed-loop PID and PI temperature control
- Fast heating followed by controlled temperature holding
- Temperature-rate filtering and over-temperature protection
- Environmental sensing for temperature, humidity, and pressure
- Wi-Fi and MQTT communication
- Serial data output for debugging
- PC-side data logging and experimental record storage
- Simulink thermal modelling
- LTspice power-stage verification

## Thermal Control Method

The heater controller uses a staged control strategy.

During the early heating stage, the system applies a high PWM duty cycle to raise the cylinder temperature quickly. As the measured temperature approaches the target, the controller changes to closed-loop PID control. The derivative term uses the measured temperature rate to reduce heater output before thermal inertia causes excessive overshoot.

Near the target temperature, the controller enters a holding state. Heater output is temporarily removed while stored heat continues to raise the cylinder temperature. After the temperature begins to fall, a conservative PI controller supplies only the power required to maintain the target.

The source code also includes:

- integral limiting
- PWM output limiting
- temperature-rate monitoring
- sensor-fault handling
- over-temperature shutdown
- automatic switching between heating and holding states

## Main Hardware

The implementation is based on the following main devices:

- ESP32 development board
- PT100 RTD temperature sensor
- MAX31865 RTD-to-digital converter
- MOSFET heater driver
- resistive heating element
- metal test cylinder
- BME280 environmental sensor
- wind-speed measurement equipment
- wind-tunnel or controlled airflow source

Additional display, buzzer, Wi-Fi, and MQTT test projects may also be included where relevant.

## Simulation

### Simulink

The Simulink models describe the relationship between PWM duty cycle, electrical power, thermal storage, and heat loss.

The average heater power can be represented as:

```text
P_avg = D × V² / R
```

The thermal response is represented by a first-order thermal model:

```text
C × dT/dt = P_avg - (T - T_env) / R_th
```

The thermal-module version uses Simscape blocks such as:

- Thermal Mass
- Thermal Resistance
- Convective Heat Transfer
- Temperature Source
- Temperature Sensor
- Thermal Reference
- Solver Configuration

These models are used to examine natural cooling, wind-speed effects, fabric insulation, and changes in thermal resistance under wet conditions.

### LTspice

The LTspice files are used to check the electrical power stage, including:

- MOSFET switching behaviour
- heater current
- resistor power
- gate-drive conditions
- device library compatibility

## Experimental Workflow

A typical experiment follows this sequence:

1. Assemble the cylinder, heater, PT100 sensor, and fabric sample.
2. Start the ESP32 acquisition and heater-control firmware.
3. Heat the cylinder to the required starting temperature.
4. Place the test sample under the selected airflow condition.
5. Record temperature, humidity, pressure, and wind-related test information.
6. Repeat the test for each fabric and environmental condition.
7. Export the recorded data for comparison and analysis.

The project is designed for comparative testing rather than medical or human-body temperature prediction. Its purpose is to provide a repeatable embedded platform for evaluating relative heat-loss behaviour under controlled environmental conditions.

## Typical Test Variables

The platform can be used to compare:

- different fabric materials
- dry and moisture-exposed samples
- multiple wind speeds
- different cooling rates
- early transient temperature response
- time required to reach a selected threshold
- repeatability across repeated trials

## Data Analysis

The recorded data can be used to calculate or compare:

- initial cooling rate
- temperature drop over a fixed time window
- cooling coefficient
- thermal time constant
- threshold time
- area under the temperature-excess curve
- mean and standard deviation across repeated trials

Results should be analysed separately for each fabric, moisture condition, and wind speed so that the effect of each variable remains clear.

## Building the ESP32 Projects

Each ESP32 project should be built from its own directory. A typical ESP-IDF workflow is:

```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

The required target may differ between subprojects. Check the source files and project configuration before flashing.

Wi-Fi credentials, MQTT settings, GPIO assignments, sensor parameters, and control constants are defined inside the corresponding project files.

## Notes

- Use the correct ESP-IDF target for the connected board.
- Check all GPIO assignments before powering the hardware.
- Confirm the PT100 wiring mode and reference resistance used by the MAX31865.
- Keep the high-current heater path separate from the sensor wiring where possible.
- Verify the MOSFET and heater power ratings before testing.
- Do not leave the heater running without temperature monitoring.
- Stop the test immediately if the measured temperature exceeds the intended safety limit.

## Report Use

The files in this directory support the implementation and appendix sections of the project report. They provide the source code, simulation models, circuit files, and data-collection tools used to build and evaluate the experimental platform.

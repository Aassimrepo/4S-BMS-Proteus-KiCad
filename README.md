# 4S Battery Management System (BMS)

### Proteus Simulation & KiCad PCB Design

A simulation-based **4S Battery Management System (BMS)** designed to monitor individual battery cells, estimate pack state, detect abnormal operating conditions, and control passive cell balancing.

The project combines **Arduino-based embedded control, Proteus circuit simulation, and KiCad PCB design** to develop and validate the BMS architecture before physical hardware fabrication.

---

## Overview

A Battery Management System is responsible for monitoring and protecting rechargeable battery packs during charging and discharging.

This project implements a **4-series-cell (4S) BMS**, where each individual cell is monitored independently. The system measures cell voltage, pack voltage, current, and temperature while providing protection and balancing functionality.

The complete design was developed and tested in simulation using **Proteus**, while the corresponding PCB was designed and routed using **KiCad**.

> **Note:** This is a simulation and PCB-design project. No physical battery hardware is used in the current implementation.

---

## Key Features

* Individual voltage monitoring for all **4 battery cells**
* Total battery pack voltage calculation
* Battery **State of Charge (SOC)** estimation
* Overvoltage protection
* Undervoltage protection
* Overcurrent detection
* Overtemperature detection
* Passive cell-balancing control
* Individual cell-balancing indicators
* Charge and load status indication
* Fault indication
* ACS712-based current sensing model
* NTC thermistor temperature sensing
* Nokia 5110 LCD monitoring
* Arduino-based embedded control
* Serial telemetry between controllers
* Complete KiCad schematic and PCB layout
* PCB routing and Design Rules Check (**DRC**)

---

## System Architecture

```text
                     ┌──────────────────────┐
                     │      4S Battery      │
                     │   Cell 1 - Cell 4   │
                     └──────────┬───────────┘
                                │
                                ▼
                     ┌──────────────────────┐
                     │   Voltage Divider    │
                     │   Sensing Network    │
                     └──────────┬───────────┘
                                │
                                ▼
                 ┌──────────────────────────────┐
                 │      Arduino BMS Controller  │
                 │                              │
                 │ • Cell Voltage Monitoring    │
                 │ • Pack Voltage Calculation   │
                 │ • SOC Estimation             │
                 │ • Fault Detection            │
                 │ • Balancing Control          │
                 └───────┬──────────┬───────────┘
                         │          │
              ┌──────────┘          └───────────┐
              ▼                                 ▼
      ┌───────────────┐                 ┌───────────────┐
      │  Nokia 5110   │                 │ Status / Fault│
      │     LCD       │                 │     LEDs      │
      └───────────────┘                 └───────────────┘
                         │
                         ▼
                 ┌─────────────────┐
                 │ Secondary MCU / │
                 │ Telemetry Link  │
                 └─────────────────┘
```

---

## Cell Voltage Measurement

The four battery-cell inputs are measured using resistor-divider networks connected to the Arduino's analog inputs.

The divider networks allow the cumulative battery tap voltages to be scaled into a measurable ADC range.

| Cell   | Divider Network | Software Scaling |
| ------ | --------------- | ---------------: |
| Cell 1 | 10k / 10k       |               ×2 |
| Cell 2 | 20k / 10k       |               ×3 |
| Cell 3 | 30k / 10k       |               ×4 |
| Cell 4 | 40k / 10k       |               ×5 |

The controller converts the measured ADC values into cell-voltage estimates and calculates the pack voltage from the four cell measurements.

---

## Battery Monitoring

The system continuously monitors:

* Cell 1 voltage
* Cell 2 voltage
* Cell 3 voltage
* Cell 4 voltage
* Pack voltage
* State of Charge (SOC)
* Battery current
* Battery temperature

ADC averaging and software filtering are used to reduce measurement fluctuations and reject invalid readings during simulation.

---

## Protection System

The BMS evaluates the measured parameters against configurable safety thresholds.

### Overvoltage

Each cell voltage is independently checked against the configured overvoltage threshold.

### Undervoltage

Each cell is monitored for voltage falling below the configured undervoltage threshold.

### Overcurrent

The ACS712 sensor model provides the current measurement. The calculated current is compared with the configured current limit.

### Overtemperature

An NTC thermistor model is used to estimate battery temperature. The system generates an overtemperature condition when the configured temperature threshold is exceeded.

### Fault Indication

When a protection condition is detected, the BMS updates its fault state and controls the corresponding protection/status outputs.

---

## Passive Cell Balancing

The BMS implements simulated passive balancing for each individual cell.

```text
Cell 1 ──► BAL1
Cell 2 ──► BAL2
Cell 3 ──► BAL3
Cell 4 ──► BAL4
```

Each balancing channel can be activated independently depending on the measured cell voltage.

The balancing state is also encoded into a telemetry value for communication to the secondary controller.

---

## LCD Interface

A **Nokia 5110 LCD** provides local monitoring of the simulated battery system.

The display provides multiple monitoring pages containing:

### Cell Monitoring

* Cell 1 voltage
* Cell 2 voltage
* Cell 3 voltage
* Cell 4 voltage
* Pack voltage
* SOC

### Current & Temperature

* Battery current
* Battery temperature

### Pack Summary

* Pack voltage
* SOC
* Telemetry status

---

## Controller Communication

The primary controller periodically transmits monitoring information to the secondary controller through serial communication.

The telemetry packet contains:

```text
Cell 1
Cell 2
Cell 3
Cell 4
Pack Voltage
SOC
Current
Temperature
Fault Code
Balancing Code
```

### Fault Codes

| Code | Condition       |
| ---: | --------------- |
|    0 | No Fault        |
|    1 | Overvoltage     |
|    2 | Undervoltage    |
|    3 | Overcurrent     |
|    4 | Overtemperature |

The balancing status is represented using a bit-coded value, allowing the state of the four balancing channels to be transmitted efficiently.

---

## Proteus Simulation

The complete BMS control system is simulated in **Proteus**.

The simulation models:

* Battery-cell voltage inputs
* Voltage-divider sensing circuits
* Arduino controllers
* ACS712 current sensor
* NTC thermistor
* Nokia 5110 LCD
* Status LEDs
* Cell-balancing outputs
* Serial communication

Different operating conditions can be introduced by changing the simulated sensor and battery inputs.

This allows the BMS control logic to be evaluated without physical battery hardware.

---

## PCB Design

The corresponding BMS PCB was designed using **KiCad**.

The PCB design includes the circuitry required for:

* Battery connections
* Cell-voltage sensing
* Current sensing
* Temperature sensing
* Microcontroller interfaces
* Cell balancing
* Status indicators
* Buzzer
* Display interface
* Communication interfaces
* ESP8266 interface

The PCB was routed and subsequently checked using KiCad's Design Rules Checker.

---

## PCB Validation

The completed PCB layout was subjected to KiCad's **Design Rules Check (DRC)**.

### Final DRC Result

```text
0 Violations
```

This confirms that the final PCB layout passed the configured KiCad design-rule checks.

---

## Repository Structure

```text
4S-BMS-Proteus-KiCad/
│
├── Proteus/
│   ├── Simulation/
│   │   ├── *.dsn
│   │   └── *.pdsprj
│   │
│   ├── Arduino_Code/
│   │   ├── *.ino
│   │   └── ...
│   │
│   └── HEX_Files/
│       └── *.hex
│
├── KiCad_PCB/
│   ├── Source/
│   │   ├── *.kicad_pro
│   │   ├── *.kicad_sch
│   │   └── *.kicad_pcb
│   │
│   └── Outputs/
│       ├── PCB_3D_Render.png
│       └── PCB_Layout.png
│
├── Documentation/
│   ├── Screenshots/
│   └── Results/
│
├── .gitignore
└── README.md
```

---

## Testing

The simulated BMS can be evaluated under different operating conditions.

| Test Condition   | Input                            | Expected Behaviour             |
| ---------------- | -------------------------------- | ------------------------------ |
| Normal Operation | Normal cell voltages             | Normal monitoring              |
| Overvoltage      | Cell exceeds OV limit            | Fault/protection indication    |
| Undervoltage     | Cell falls below UV limit        | Load protection/fault          |
| Overcurrent      | Current exceeds limit            | Overcurrent protection         |
| Overtemperature  | Temperature exceeds limit        | Temperature protection         |
| Cell Balancing   | Cell reaches balancing threshold | Corresponding balancing output |
| Recovery         | Fault condition removed          | Return to normal operation     |

Screenshots demonstrating the simulation and PCB validation are available in the `Documentation/Screenshots` directory.

---

## Technologies Used

### Hardware / Controllers

* Arduino Uno
* Arduino Nano
* ESP8266 interface
* ACS712 current sensor
* NTC thermistor
* Nokia 5110 LCD
* Passive balancing circuitry

### Software

* Proteus
* KiCad
* Arduino IDE
* C/C++

---

## Future Development

The project can be extended into an IoT-enabled battery monitoring platform.

Planned extensions include:

* Raspberry Pi 5 IoT gateway
* Real-time web dashboard
* Remote BMS telemetry
* Live voltage/current/temperature graphs
* Historical data logging
* Battery-state prediction
* Machine-learning based battery analysis
* Remote fault notifications
* Physical hardware implementation and validation

These features are intended as future extensions and are **not represented as completed functionality in the current version of the repository**.

---

## Project Documentation

The repository contains screenshots showing:

* Complete Proteus simulation
* Normal BMS operation
* Fault detection
* PCB routing
* PCB 3D visualization
* KiCad DRC results

Refer to:

```text
Documentation/Screenshots/
```

for the available visual documentation.

---

## Disclaimer

This project is intended for **educational and research purposes**.

The current implementation is a simulation and PCB-design study and has not been validated on physical battery hardware.

A real battery-management system requires appropriate component selection, isolation, protection circuitry, thermal considerations, battery characterization, hardware validation, and extensive safety testing before being used with a real battery pack.

---

## Author

**Aassim Basheer**

Electronics and Communication Engineering

---

## License

This project is provided for educational and research purposes.

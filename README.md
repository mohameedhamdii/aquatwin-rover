# AquaTwin — Autonomous Water-Quality Monitoring Rover & 3D Digital Twin

[![Firmware](https://img.shields.io/badge/Firmware-STM32L4%20%2B%20FreeRTOS-blue.svg)](file:///firmware)
[![Language](https://img.shields.io/badge/Language-C11%20%7C%20Python%20%7C%20JS-success.svg)](#)
[![Comms](https://img.shields.io/badge/Comms-LoRa%20868%2F915%20MHz-orange.svg)](file:///docs/lora_setup.md)
[![Storage](https://img.shields.io/badge/Database-InfluxDB%202.x-purple.svg)](#)
[![Visualisation](https://img.shields.io/badge/Frontend-Three.js%203D%20Digital%20Twin-cyan.svg)](file:///dashboard)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](file:///LICENSE)

> **A solar-powered aquatic surface robot and anchored buoy that patrols waterways, measures chemical contamination indicators, runs an on-device Extended Kalman Filter (EKF) for statistical anomaly detection, and streams live telemetry to an interactive 3D digital-twin dashboard.**

---

## System Architecture

```
┌────────────────────────────────────────────────────────────────────────┐
│                      AQUATWIN FIELD UNIT (ROVER / BUOY)                │
│                                                                        │
│   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌───────────┐  │
│   │  pH Sensor   │  │  Turbidity   │  │  DO Sensor   │  │  DS18B20  │  │
│   │ (SEN0161-V2) │  │  (SEN0189)   │  │ (SEN0237-A)  │  │ (1-Wire)  │  │
│   └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └─────┬─────┘  │
│          │ ADC             │ ADC             │ ADC            │ GPIO   │
│          ▼                 ▼                 ▼                ▼        │
│   ┌────────────────────────────────────────────────────────────────┐   │
│   │             STM32L476RG (ARM Cortex-M4 @ 80 MHz)               │   │
│   │  • FreeRTOS Multi-Tasking Architecture                         │   │
│   │  • 10 Hz Oversampling + 1 Hz Windowed Sensor Averaging         │   │
│   │  • Nernst Temperature-Compensated Calibration                  │   │
│   │  • Extended Kalman Filter (4-State Vector Fusion)              │   │
│   │  • Normalized Innovation Squared (NIS) Anomaly Detection       │   │
│   │  • 32-Byte Fixed-Point Binary Telemetry Assembler              │   │
│   │  • STOP2 Deep Sleep (~2 µA) + Adaptive Solar Interval Power    │   │
│   └────────────────────────────────┬───────────────────────────────┘   │
│                                    │                                   │
│                  ┌─────────────────┴─────────────────┐                 │
│                  ▼                                   ▼                 │
│         ┌─────────────────┐                 ┌─────────────────┐        │
│         │ SX1276 LoRa     │                 │ DRV8833 Dual    │        │
│         │ (868 / 915 MHz) │                 │ Thruster Nav    │        │
│         └────────┬────────┘                 └─────────────────┘        │
└──────────────────┼─────────────────────────────────────────────────────┘
                   │ Up to 10+ km LoRa RF Link
                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│                   LoRa GATEWAY & TELEMETRY INGESTION                   │
│   RAK7268 / Raspberry Pi HAT / USB UART Serial Receiver                │
└──────────────────┬─────────────────────────────────────────────────────┘
                   │ HTTP POST (32-byte Hex Frame)
                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│                   BACKEND SERVICE & REAL-TIME WEBSOCKET                │
│   ┌───────────────────────────┐         ┌──────────────────────────┐   │
│   │ FastAPI Telemetry Engine  │────────▶│ InfluxDB 2.x Time-Series │   │
│   │ (State Decoding & Alerts) │         │ (SQLite Auto-Fallback)   │   │
│   └──────────────┬────────────┘         └──────────────────────────┘   │
└──────────────────┼─────────────────────────────────────────────────────┘
                   │ WebSocket (/ws/live) @ 1 Hz
                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│              AQUATWIN 3D DIGITAL-TWIN DASHBOARD (THREE.JS)             │
│   • Procedural Water Shader with Dynamic Wave Ripples & Specular Glint │
│   • Real-Time Rover GPS Path Tracking & Anomaly Beacon Strobe          │
│   • Glassmorphic HUD Gauges (pH, Turbidity, DO, Temp, Power)           │
│   • Rolling Multi-Metric Time-Series Trend Charts                      │
│   • Contamination Alarm Feed & Real-Time Parameter Anomaly Diagnostics │
└────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Bill of Materials (BOM)

| Component | Part / Model Number | Qty | Purpose | Interface |
|---|---|---|---|---|
| **Microcontroller** | STM32L476RG (Nucleo-64) | 1 | Sensor reading, EKF fusion, LoRa TX, FreeRTOS | ARM Cortex-M4 |
| **pH Sensor** | DFRobot Gravity V2 (SEN0161-V2) | 1 | Water acidity/alkalinity measurement | Analog (PA0 / ADC1_IN5) |
| **Turbidity Sensor** | DFRobot Gravity (SEN0189) | 1 | Suspended particulate & clarity monitor | Analog (PA1 / ADC1_IN6) |
| **Dissolved Oxygen**| DFRobot Gravity (SEN0237-A) | 1 | Dissolved oxygen concentration (mg/L) | Analog (PA4 / ADC1_IN9) |
| **Water Temp** | Maxim DS18B20 Waterproof Probe | 1 | Water temperature & Nernst pH compensation | 1-Wire (PB0) |
| **LoRa Radio** | Semtech SX1276 / RFM95W | 1 | Long-range low-power telemetry transmission | SPI1 (PA5/6/7, PB6, PC7, PB4) |
| **GPS Module** | u-blox NEO-M8N | 1 | Geolocation for autonomous rover patrol | UART2 (PA2/PA3, 9600 baud) |
| **Solar Panel** | 6V / 3W Monocrystalline Mini Panel | 1 | Outdoor renewable energy harvesting | Solar DC input |
| **Solar MPPT** | DFRobot Solar Power Manager (DFR0559) | 1 | MPPT charge management for 3.7V LiPo | CN3065 controller |
| **Battery** | 3.7V 6000 mAh Single-Cell LiPo | 1 | Energy storage for continuous night operation | JST-PH 2.0 |
| **Motor Driver** | DRV8833 Dual H-Bridge | 1 | Differential thruster propulsion (rover mode) | PWM (TIM4_CH3/4) |
| **Thrusters** | Submersible 12V/5V Marine Thrusters | 2 | Autonomous steering and waypoint locomotion | Brushed DC |
| **Enclosure** | IP67 Waterproof Junction Box | 1 | Main electronics & battery waterproof chamber| Cable Glands PG7/PG9 |

---

## Key Technical Features

### 1. Extended Kalman Filter (EKF) Sensor Fusion
Water quality sensors in natural environments suffer from hydrodynamic noise, sediment turbulence, and probe response lag.
- **State Vector:** $\mathbf{x} = [\text{pH}, \text{Turbidity}, \text{DO}, \text{Temperature}]^T$.
- **Process Covariance:** $\mathbf{Q} = \text{diag}(0.01, 0.5, 0.05, 0.01)$.
- **Measurement Covariance:** $\mathbf{R} = \text{diag}(0.05, 2.0, 0.1, 0.02)$.
- **Statistical Innovation Test:** Evaluates the Normalized Innovation Squared ($NIS = \mathbf{y}^T \mathbf{S}^{-1} \mathbf{y}$).
- If $NIS > 13.28$ ($\chi^2$ 4-DOF 99% confidence threshold), the firmware flags an immediate **Statistical Contamination Anomaly**, alerting operators before long-term environmental degradation occurs.

### 2. High-Efficiency 32-Byte Packed Telemetry
The firmware serializes all sensor states, GPS coordinates, power status, EKF metrics, and flags into a **32-byte fixed-point frame**:
```c
typedef struct __attribute__((packed)) {
    uint32_t timestamp;         // Unix epoch (4 bytes)
    int32_t  latitude;          // Lat × 1e7 (4 bytes)
    int32_t  longitude;         // Lon × 1e7 (4 bytes)
    int16_t  ph_x100;           // pH × 100 (2 bytes)
    int16_t  turbidity_x10;     // NTU × 10 (2 bytes)
    int16_t  do_x100;           // DO mg/L × 100 (2 bytes)
    int16_t  temp_x100;         // Temp °C × 100 (2 bytes)
    uint16_t battery_mv;        // Battery mV (2 bytes)
    uint8_t  solar_power_pct;   // Solar % (1 byte)
    uint8_t  anomaly_flags;     // Anomaly bitmask (1 byte)
    uint16_t ekf_nis_x100;      // NIS × 100 (2 bytes)
    uint8_t  device_id;         // Node ID (1 byte)
    uint8_t  packet_version;    // Version 0x01 (1 byte)
    uint8_t  reserved[4];       // Expansion (4 bytes)
} wq_packet_t;                  // Total: exactly 32 bytes
```

### 3. Adaptive Power Management
- **Normal Operation:** Full sensor reads + 60s LoRa transmission + STOP2 deep sleep ($2\ \mu\text{A}$ consumption between cycles).
- **Low Power Mode (< 20% Battery):** Backs off transmission rate to 300 seconds, disables motor thrusters.
- **Critical Mode (< 10% Battery):** Enters standby sleep until solar panel charges LiPo above 4.0V.

---

## Project Directory Layout

```
waterquality/
├── firmware/
│   ├── CMakeLists.txt                # ARM cross-compilation & native test config
│   ├── STM32L476RGTx_FLASH.ld        # STM32 Flash & SRAM linker script
│   ├── config/
│   │   ├── FreeRTOSConfig.h          # FreeRTOS kernel configuration
│   │   ├── calibration.h             # Two-point sensor calibration constants
│   │   ├── lora_config.h             # Frequency, SF, BW, TX power settings
│   │   └── pin_map.h                 # MCU peripheral pin mapping definitions
│   ├── src/
│   │   ├── main.c                    # FreeRTOS task orchestration & entry point
│   │   ├── system_clock.c            # 80 MHz MSI+PLL clock configuration
│   │   ├── stm32l4xx_hal_msp.c       # HAL GPIO & peripheral clock setup
│   │   ├── stm32l4xx_it.c            # EXTI & UART interrupt service routines
│   │   ├── freertos_hooks.c          # Idle, tick, stack overflow handlers
│   │   ├── sensors/                  # pH, Turbidity, DO, DS18B20 drivers
│   │   ├── ekf/                      # Kalman filter & innovation anomaly detector
│   │   ├── comms/                    # SX1276 LoRa driver, packet serializer, satellite
│   │   ├── power/                    # LiPo SoC estimator, solar monitor, sleep manager
│   │   └── nav/                      # u-blox GPS parser, DRV8833 motors, waypoints
│   └── tests/
│       ├── CMakeLists.txt            # Native unit test build file
│       ├── test_ekf.c                # EKF convergence and NIS unit tests
│       ├── test_sensors.c            # Sensor calibration math unit tests
│       └── test_packet.c             # Packet binary alignment and round-trip tests
├── backend/
│   ├── main.py                       # FastAPI application & WebSocket server
│   ├── config.yaml                   # Server, LoRa, InfluxDB, simulator config
│   ├── requirements.txt              # Python dependencies
│   ├── Dockerfile                    # Container build file
│   ├── api/
│   │   ├── routes.py                 # REST endpoints for data, alerts, waypoints
│   │   └── models.py                 # Pydantic telemetry & alert models
│   ├── services/
│   │   ├── data_service.py           # 32-byte packet decoding & mock generator
│   │   └── influx_client.py          # InfluxDB 2.x client + SQLite auto-fallback
│   └── tests/
│       └── test_backend.py           # Automated API & unpacking test suite
├── dashboard/
│   ├── index.html                    # Glassmorphic HUD & 3D digital-twin interface
│   ├── css/style.css                 # Oceanic design system, glassmorphism tokens
│   └── js/
│       ├── main.js                   # Three.js 3D scene, catamaran rover, lighting
│       ├── water.js                  # Dynamic procedural water wave shader
│       ├── data-overlay.js           # Live gauges, Chart.js trends, anomaly banner
│       └── websocket.js              # Resilient WebSocket connection manager
├── gateway/
│   ├── lora_receiver.py              # Hardware serial/SPI LoRa forwarder script
│   ├── config.yaml                   # Serial port & baudrate configuration
│   └── requirements.txt              # Gateway dependencies
├── hardware/
│   ├── schematic.md                  # Comprehensive electrical wiring documentation
│   ├── wiring_diagram.svg            # High-resolution vector wiring schematic
│   └── enclosure_stl/
│       ├── electronics_box.stl       # 3D printable IP67 junction box CAD model
│       └── buoy_hull.stl             # 3D printable catamaran float CAD model
├── docs/
│   ├── sensor_calibration_guide.md   # Step-by-step buffer calibration guide
│   ├── lora_setup.md                 # RF rules, spreading factors, TTN decoders
│   └── deployment_guide.md           # Field deployment & bucket test instructions
├── docker-compose.yaml               # InfluxDB + Backend stack orchestration
└── LICENSE                           # MIT License
```

---

## Quick Start Guide

### 1. Running the Backend & 3D Dashboard Locally

```bash
# 1. Install dependencies
cd backend
pip install -r requirements.txt

# 2. Start the telemetry server
python main.py
```

Open your browser to:
- **3D Digital-Twin Dashboard:** `http://localhost:8000/dashboard/`
- **Interactive Swagger API Docs:** `http://localhost:8000/docs`

> **Real-Time Hardware Operation:** The backend and 3D dashboard run in pure hardware telemetry mode. When waiting for incoming LoRa packets from field nodes, the HUD cleanly indicates `AWAITING NODE PACKET` with gauges in standby. As physical packets arrive via the LoRa gateway or `/api/v1/packet/raw`, the digital twin updates live with verified sensor readings, GPS positions, and signal stats.

### 2. Ingesting Real Telemetry from Hardware
Transmitting a real 32-byte binary packet from the field LoRa receiver:
```bash
# Gateway forwards binary packets directly from USB UART receiver:
python gateway/lora_receiver.py
```
Or forward a raw packet directly to the API endpoint:
```bash
curl -X POST http://localhost:8000/api/v1/packet/raw \
  -H "Content-Type: application/json" \
  -d '{"hex_payload": "a0210000204eaf1e7047ecf8df0255001c039007c00e5200008400010100000000", "rssi": -78}'
```
Observe the live response on the dashboard:
1. Status chip switches to **REAL-TIME HARDWARE FEED** with green indicator and current RSSI.
2. Gauges immediately display genuine pH, turbidity, dissolved oxygen, water temperature, and battery/solar levels.
3. If contamination thresholds or statistical EKF NIS divergence are triggered, the beacon turns crimson red with on-screen anomaly flags.
4. Historical trends are continuously recorded into InfluxDB / SQLite.

### 3. Running Backend Automated Tests
```bash
pytest backend/tests/test_backend.py -v
```

### 4. Running the LoRa Gateway Forwarder
When connected to an SX1276 receiver via USB serial (e.g. `COM3` or `/dev/ttyUSB0`):
```bash
python gateway/lora_receiver.py
```
To run in simulated demo mode without physical LoRa hardware:
```bash
python gateway/lora_receiver.py --demo
```

### 5. Compiling the STM32 Firmware
```bash
cd firmware
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake -DBUILD_MODE_ROVER=ON ..
make -j4
```

---

## Sensor Calibration Summary

| Sensor | Calibration Method | Nominal Values |
|---|---|---|
| **pH Sensor** | Two-point buffer calibration in pH 7.00 & pH 4.00 standard solutions | Neutral: 1500 mV, Acid: 2030 mV |
| **Turbidity Sensor** | Optical reference in pure distilled water & 1000 NTU Formazin standard | Clean: ~4.1V, Turbid: ~2.5V |
| **DO Sensor** | Single-point reference in air-saturated water bubbled for 15 min | Ref: 1600 mV @ 20°C (9.09 mg/L) |
| **Temperature** | Factory-calibrated Maxim 1-Wire protocol | $0.0625\text{ °C}$ resolution |

*Refer to [Sensor Calibration Guide](file:///docs/sensor_calibration_guide.md) for full laboratory calibration steps.*

---

## License

This project is licensed under the **MIT License** — see the [LICENSE](file:///LICENSE) file for details.

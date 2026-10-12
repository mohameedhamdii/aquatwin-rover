# AquaTwin Hardware Schematic & Electrical Wiring Guide

## 1. Microcontroller Pinout Reference (STM32L476RG / Nucleo-64)

| Peripheral / Module | Signal Name | MCU Pin | Pin Function | Electrical Characteristics & Notes |
|---|---|---|---|---|
| **pH Sensor** (SEN0161-V2) | pH Analog Out | `PA0` | `ADC1_IN5` | 12-bit ADC, 0–3.3V analog input |
| **Turbidity** (SEN0189) | Turbidity Analog | `PA1` | `ADC1_IN6` | 12-bit ADC, optical phototransistor voltage |
| **Dissolved O₂** (SEN0237-A) | DO Analog Out | `PA4` | `ADC1_IN9` | 12-bit ADC, galvanic/polarographic output |
| **Water Temp** (DS18B20) | OneWire Data | `PB0` | `GPIO_Input/OD` | External 4.7 kΩ pull-up resistor to 3.3V |
| **Battery Voltage Sense** | V_BATT Divider | `PC0` | `ADC1_IN1` | 100 kΩ / 100 kΩ divider (divides Vbatt by 2) |
| **Solar Voltage Sense** | V_SOLAR Divider | `PC1` | `ADC1_IN2` | 100 kΩ / 47 kΩ divider (safe for up to 10V panels)|
| **LoRa SCK** (SX1276) | SPI SCK | `PA5` | `SPI1_SCK` | High-speed SPI clock (up to 10 MHz) |
| **LoRa MISO** (SX1276) | SPI MISO | `PA6` | `SPI1_MISO` | SPI Master In / Slave Out |
| **LoRa MOSI** (SX1276) | SPI MOSI | `PA7` | `SPI1_MOSI` | SPI Master Out / Slave In |
| **LoRa NSS / CS** | Chip Select | `PB6` | `GPIO_Output` | Active Low select line |
| **LoRa RESET** | Reset | `PC7` | `GPIO_Output` | Active Low hardware reset |
| **LoRa DIO0** | Packet RX/TX IRQ| `PB4` | `EXTI4` | Rising-edge interrupt trigger on DIO0 |
| **GPS Module** (u-blox) | GPS TX -> MCU RX| `PA3` | `USART2_RX` | 9600 baud NMEA sentences |
| **GPS Module** (u-blox) | GPS RX <- MCU TX| `PA2` | `USART2_TX` | Configuration and UBX polling |
| **Motor Left PWM** (DRV8833) | Left Thruster | `PB8` | `TIM4_CH3` | 20 kHz PWM differential speed control |
| **Motor Right PWM** (DRV8833)| Right Thruster | `PB9` | `TIM4_CH4` | 20 kHz PWM differential speed control |
| **Motor Sleep** (DRV8833) | Motor Wake/Sleep| `PC8` | `GPIO_Output` | Low = Sleep (ultra-low quiescent current) |
| **Status Beacon LED** | RGB / Beacon | `PB13` | `GPIO_Output` | Active High visual status indicator |

---

## 2. Power Architecture & Solar Harvesting

```
                     ┌───────────────────────┐
                     │ 6V / 3W Mini Solar    │
                     │ Photovoltaic Panel    │
                     └───────────┬───────────┘
                                 │ V_IN (4.5V - 6.5V)
                                 ▼
                     ┌───────────────────────┐
                     │ DFR0559 Solar Power   │
                     │ Manager (MPPT CN3065) │
                     └─────┬───────────┬─────┘
       Battery Port (3.7V) │           │ Regulated 5.0V / 3.3V Rail
                           ▼           ▼
             ┌─────────────────┐   ┌─────────────────────────────┐
             │ 3.7V 6000mAh    │   │ • STM32L476RG Microcontroller│
             │ Single-Cell LiPo│   │ • SX1276 LoRa Transceiver   │
             │ Rechargeable    │   │ • u-blox NEO-M8N GPS        │
             │ Battery Pack    │   │ • 4 × Submerged Sensors     │
             └─────────────────┘   │ • DRV8833 Dual Thruster H-B │
                                   └─────────────────────────────┘
```

---

## 3. Voltage Divider Formulas

### Battery Voltage Divider (`PC0`)
- Resistors: $R_1 = 100\text{ k}\Omega$, $R_2 = 100\text{ k}\Omega$ (1% tolerance)
- Ratio:
  $$V_{\text{ADC}} = V_{\text{BATT}} \times \frac{R_2}{R_1 + R_2} = V_{\text{BATT}} \times 0.5$$
- For a fully charged 4.2V LiPo: $V_{\text{ADC}} = 2.10\text{ V}$ (well within STM32 3.3V reference).

### Solar Panel Voltage Divider (`PC1`)
- Resistors: $R_1 = 100\text{ k}\Omega$, $R_2 = 47\text{ k}\Omega$ (1% tolerance)
- Ratio:
  $$V_{\text{ADC}} = V_{\text{SOLAR}} \times \frac{47}{100 + 47} = V_{\text{SOLAR}} \times 0.3197$$
- For a 6.5V solar peak: $V_{\text{ADC}} = 2.08\text{ V}$.

---

## 4. Waterproofing & Physical Enclosure Guide
1. **Cable Glands (PG7 / PG9):** Drill holes in the bottom side of the IP67 junction box to route sensor probe cables. Never route cables through the top to prevent pooling water from leaking in.
2. **Silicone Grease & O-Rings:** Coat IP67 lid neoprene gasket with marine silicone grease prior to closing.
3. **Desiccant Packets:** Place 2 × 5g silica gel packets inside the enclosure to absorb condensation caused by day/night temperature cycling.
4. **Conformal Coating:** Spray silicone or acrylic conformal coating (MG Chemicals 422B) on all exposed prototype PCB traces and solder joints.

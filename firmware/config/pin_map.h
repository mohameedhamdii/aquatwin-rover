/**
 * @file pin_map.h
 * @brief GPIO and peripheral pin assignments for the Water Quality Monitor
 *
 * Target: STM32L476RG Nucleo-64
 * All pin assignments are centralised here for easy re-mapping.
 */

#ifndef PIN_MAP_H
#define PIN_MAP_H

#include "stm32l4xx_hal.h"

/* ============================================================================
 * ADC Channels (ADC1)
 * ============================================================================ */

/* pH Sensor — DFRobot SEN0161-V2 analog output */
#define PH_SENSOR_ADC_CHANNEL       ADC_CHANNEL_5       /* PA0 → ADC1_IN5  */
#define PH_SENSOR_GPIO_PORT         GPIOA
#define PH_SENSOR_GPIO_PIN          GPIO_PIN_0

/* Turbidity Sensor — DFRobot SEN0189 analog output */
#define TURB_SENSOR_ADC_CHANNEL     ADC_CHANNEL_6       /* PA1 → ADC1_IN6  */
#define TURB_SENSOR_GPIO_PORT       GPIOA
#define TURB_SENSOR_GPIO_PIN        GPIO_PIN_1

/* Dissolved Oxygen Sensor — DFRobot SEN0237-A analog output */
#define DO_SENSOR_ADC_CHANNEL       ADC_CHANNEL_9       /* PA4 → ADC1_IN9  */
#define DO_SENSOR_GPIO_PORT         GPIOA
#define DO_SENSOR_GPIO_PIN          GPIO_PIN_4

/* Battery voltage monitor (via voltage divider) */
#define BATTERY_ADC_CHANNEL         ADC_CHANNEL_10      /* PA5 → ADC1_IN10 */
#define BATTERY_GPIO_PORT           GPIOA
#define BATTERY_GPIO_PIN            GPIO_PIN_5

/* Solar panel voltage monitor (via voltage divider) */
#define SOLAR_ADC_CHANNEL           ADC_CHANNEL_11      /* PA6 → ADC1_IN11 */
#define SOLAR_GPIO_PORT             GPIOA
#define SOLAR_GPIO_PIN              GPIO_PIN_6

/* ============================================================================
 * OneWire — DS18B20 Temperature Sensor
 * ============================================================================ */
#define DS18B20_GPIO_PORT           GPIOB
#define DS18B20_GPIO_PIN            GPIO_PIN_0
#define DS18B20_TIMER               TIM2            /* Microsecond timing for OneWire */
#define DS18B20_TIMER_INSTANCE      TIM2

/* ============================================================================
 * SPI1 — LoRa (SX1276 / RFM95W)
 * ============================================================================ */
#define LORA_SPI_INSTANCE           SPI1
#define LORA_SPI_SCK_PORT           GPIOA
#define LORA_SPI_SCK_PIN            GPIO_PIN_5       /* Remapped: PA5 used by ADC, use PB3 */
/* NOTE: SPI1 alternate mapping on Nucleo-64 */
#define LORA_SPI_MISO_PORT          GPIOA
#define LORA_SPI_MISO_PIN           GPIO_PIN_6
#define LORA_SPI_MOSI_PORT          GPIOA
#define LORA_SPI_MOSI_PIN           GPIO_PIN_7
/* Corrected SPI1 pin mapping to avoid ADC conflicts */
#undef LORA_SPI_SCK_PORT
#undef LORA_SPI_SCK_PIN
#undef LORA_SPI_MISO_PORT
#undef LORA_SPI_MISO_PIN
#undef LORA_SPI_MOSI_PORT
#undef LORA_SPI_MOSI_PIN
#define LORA_SPI_SCK_PORT           GPIOB
#define LORA_SPI_SCK_PIN            GPIO_PIN_3
#define LORA_SPI_MISO_PORT          GPIOB
#define LORA_SPI_MISO_PIN           GPIO_PIN_4
#define LORA_SPI_MOSI_PORT          GPIOB
#define LORA_SPI_MOSI_PIN           GPIO_PIN_5

#define LORA_NSS_PORT               GPIOB
#define LORA_NSS_PIN                GPIO_PIN_6
#define LORA_RESET_PORT             GPIOC
#define LORA_RESET_PIN              GPIO_PIN_7
#define LORA_DIO0_PORT              GPIOA
#define LORA_DIO0_PIN               GPIO_PIN_9       /* Interrupt on TX/RX done */
#define LORA_DIO0_IRQn              EXTI9_5_IRQn

/* ============================================================================
 * UART2 — GPS (u-blox NEO-M8N)  [Rover Mode]
 * ============================================================================ */
#define GPS_UART_INSTANCE           USART2
#define GPS_UART_TX_PORT            GPIOA
#define GPS_UART_TX_PIN             GPIO_PIN_2
#define GPS_UART_RX_PORT            GPIOA
#define GPS_UART_RX_PIN             GPIO_PIN_3
#define GPS_UART_BAUDRATE           9600
#define GPS_UART_IRQn               USART2_IRQn

/* ============================================================================
 * UART1 — Debug / Console (via ST-LINK VCP)
 * ============================================================================ */
#define DEBUG_UART_INSTANCE         USART1
#define DEBUG_UART_TX_PORT          GPIOB
#define DEBUG_UART_TX_PIN           GPIO_PIN_6
#define DEBUG_UART_RX_PORT          GPIOB
#define DEBUG_UART_RX_PIN           GPIO_PIN_7
#define DEBUG_UART_BAUDRATE         115200

/* ============================================================================
 * Motor Driver — DRV8833  [Rover Mode]
 * ============================================================================ */
#define MOTOR_LEFT_FWD_PORT         GPIOC
#define MOTOR_LEFT_FWD_PIN          GPIO_PIN_0
#define MOTOR_LEFT_FWD_TIM          TIM1
#define MOTOR_LEFT_FWD_CH           TIM_CHANNEL_1

#define MOTOR_LEFT_REV_PORT         GPIOC
#define MOTOR_LEFT_REV_PIN          GPIO_PIN_1
#define MOTOR_LEFT_REV_TIM          TIM1
#define MOTOR_LEFT_REV_CH           TIM_CHANNEL_2

#define MOTOR_RIGHT_FWD_PORT        GPIOC
#define MOTOR_RIGHT_FWD_PIN         GPIO_PIN_2
#define MOTOR_RIGHT_FWD_TIM         TIM1
#define MOTOR_RIGHT_FWD_CH          TIM_CHANNEL_3

#define MOTOR_RIGHT_REV_PORT        GPIOC
#define MOTOR_RIGHT_REV_PIN         GPIO_PIN_3
#define MOTOR_RIGHT_REV_TIM         TIM1
#define MOTOR_RIGHT_REV_CH          TIM_CHANNEL_4

/* Motor driver enable/sleep */
#define MOTOR_SLEEP_PORT            GPIOC
#define MOTOR_SLEEP_PIN             GPIO_PIN_4

/* ============================================================================
 * Status LED
 * ============================================================================ */
#define STATUS_LED_PORT             GPIOA           /* Nucleo onboard LED (LD2) */
#define STATUS_LED_PIN              GPIO_PIN_5      /* Note: shared with SPI — remap if using SPI */

/* Use PC13 as status LED to avoid conflict */
#undef STATUS_LED_PORT
#undef STATUS_LED_PIN
#define STATUS_LED_PORT             GPIOC
#define STATUS_LED_PIN              GPIO_PIN_13

/* ============================================================================
 * RTC — Wake-up from STOP mode
 * ============================================================================ */
/* RTC uses LSE (32.768 kHz) — no GPIO needed, internal peripheral */

/* ============================================================================
 * I2C1 — Satellite Modem (optional, RockBLOCK 9603)
 * ============================================================================ */
#define SAT_I2C_INSTANCE            I2C1
#define SAT_I2C_SCL_PORT            GPIOB
#define SAT_I2C_SCL_PIN             GPIO_PIN_8
#define SAT_I2C_SDA_PORT            GPIOB
#define SAT_I2C_SDA_PIN             GPIO_PIN_9

#endif /* PIN_MAP_H */

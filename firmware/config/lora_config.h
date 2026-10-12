/**
 * @file lora_config.h
 * @brief LoRa radio configuration for the Water Quality Monitor
 *
 * Configuration for SX1276/RFM95W module.
 * EU868 region settings by default.
 */

#ifndef LORA_CONFIG_H
#define LORA_CONFIG_H

/* ============================================================================
 * Frequency Configuration
 * ============================================================================ */
#if defined(LORA_REGION_EU868)
    #define LORA_FREQUENCY_HZ           868100000UL     /* 868.1 MHz (EU g1 sub-band) */
    #define LORA_DUTY_CYCLE_PCT         1               /* 1% duty cycle (EU regulation) */
    #define LORA_MAX_TX_POWER_DBM       14              /* Max EIRP for EU868 */
#elif defined(LORA_REGION_US915)
    #define LORA_FREQUENCY_HZ           915000000UL     /* 915 MHz (US ISM band) */
    #define LORA_DUTY_CYCLE_PCT         100             /* No duty cycle limit in US */
    #define LORA_MAX_TX_POWER_DBM       20              /* Max for US915 */
#else
    #error "No LoRa region defined. Set LORA_REGION_EU868 or LORA_REGION_US915."
#endif

/* ============================================================================
 * Radio Parameters
 * ============================================================================ */
#define LORA_SPREADING_FACTOR       10              /* SF10: good range vs airtime */
#define LORA_BANDWIDTH_HZ           125000          /* 125 kHz */
#define LORA_CODING_RATE            5               /* 4/5 coding rate */
#define LORA_PREAMBLE_LENGTH        8               /* Standard preamble */
#define LORA_SYNC_WORD              0x12            /* Private network sync word */
#define LORA_TX_POWER_DBM           14              /* Transmit power (dBm) */
#define LORA_CRC_ENABLED            1

/* ============================================================================
 * Timing
 * ============================================================================ */
#define LORA_TX_INTERVAL_S          60              /* Normal transmit interval (seconds) */
#define LORA_TX_INTERVAL_LOW_BAT_S  300             /* Reduced interval when battery low */
#define LORA_TX_TIMEOUT_MS          5000            /* TX timeout (milliseconds) */
#define LORA_RX_TIMEOUT_MS          1000            /* RX timeout for ack (if used) */

/* ============================================================================
 * Packet Configuration
 * ============================================================================ */
#define LORA_MAX_PAYLOAD_SIZE       64              /* Max payload bytes */
#define LORA_WQ_PACKET_SIZE         32              /* Our water quality packet size */

/* ============================================================================
 * SX1276 Register Addresses (commonly used)
 * ============================================================================ */
#define SX1276_REG_FIFO             0x00
#define SX1276_REG_OP_MODE          0x01
#define SX1276_REG_FR_MSB           0x06
#define SX1276_REG_FR_MID           0x07
#define SX1276_REG_FR_LSB           0x08
#define SX1276_REG_PA_CONFIG        0x09
#define SX1276_REG_PA_RAMP          0x0A
#define SX1276_REG_OCP              0x0B
#define SX1276_REG_LNA              0x0C
#define SX1276_REG_FIFO_ADDR_PTR   0x0D
#define SX1276_REG_FIFO_TX_BASE    0x0E
#define SX1276_REG_FIFO_RX_BASE    0x0F
#define SX1276_REG_FIFO_RX_CURRENT 0x10
#define SX1276_REG_IRQ_FLAGS_MASK  0x11
#define SX1276_REG_IRQ_FLAGS       0x12
#define SX1276_REG_RX_NB_BYTES     0x13
#define SX1276_REG_MODEM_CONFIG_1  0x1D
#define SX1276_REG_MODEM_CONFIG_2  0x1E
#define SX1276_REG_SYMB_TIMEOUT    0x1F
#define SX1276_REG_PREAMBLE_MSB    0x20
#define SX1276_REG_PREAMBLE_LSB    0x21
#define SX1276_REG_PAYLOAD_LENGTH  0x22
#define SX1276_REG_MODEM_CONFIG_3  0x26
#define SX1276_REG_RSSI_VALUE      0x1A
#define SX1276_REG_PKT_SNR_VALUE   0x19
#define SX1276_REG_PKT_RSSI_VALUE  0x1A
#define SX1276_REG_DIO_MAPPING_1   0x40
#define SX1276_REG_VERSION         0x42
#define SX1276_REG_PA_DAC          0x4D

/* SX1276 operating modes */
#define SX1276_MODE_SLEEP           0x00
#define SX1276_MODE_STANDBY         0x01
#define SX1276_MODE_FSTX            0x02
#define SX1276_MODE_TX              0x03
#define SX1276_MODE_FSRX            0x04
#define SX1276_MODE_RX_CONTINUOUS   0x05
#define SX1276_MODE_RX_SINGLE       0x06
#define SX1276_MODE_CAD             0x07
#define SX1276_MODE_LORA            0x80
#define SX1276_MODE_ACCESS_SHARED   0x40

/* IRQ flags */
#define SX1276_IRQ_TX_DONE          0x08
#define SX1276_IRQ_RX_DONE          0x40
#define SX1276_IRQ_CRC_ERROR        0x20
#define SX1276_IRQ_VALID_HEADER     0x10
#define SX1276_IRQ_CAD_DONE         0x04
#define SX1276_IRQ_CAD_DETECTED     0x01

/* ============================================================================
 * Device ID
 * ============================================================================ */
#define LORA_DEVICE_ID              0x01            /* Unique device identifier */

#endif /* LORA_CONFIG_H */

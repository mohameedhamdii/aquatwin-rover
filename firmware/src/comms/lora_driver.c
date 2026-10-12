/**
 * @file lora_driver.c
 * @brief SX1276/RFM95W LoRa radio driver implementation
 *
 * Bare-metal SPI driver for the SX1276 in LoRa mode.
 * Handles configuration, packet TX, sleep, and interrupt processing.
 */

#include "comms/lora_driver.h"
#include "lora_config.h"
#include "pin_map.h"
#include <string.h>

/* ============================================================================
 * SPI helpers
 * ============================================================================ */

static void nss_low(lora_t *lora)
{
    HAL_GPIO_WritePin(lora->nss_port, lora->nss_pin, GPIO_PIN_RESET);
}

static void nss_high(lora_t *lora)
{
    HAL_GPIO_WritePin(lora->nss_port, lora->nss_pin, GPIO_PIN_SET);
}

uint8_t lora_read_reg(lora_t *lora, uint8_t reg)
{
    uint8_t tx[2] = { reg & 0x7F, 0x00 };
    uint8_t rx[2] = { 0 };

    nss_low(lora);
    HAL_SPI_TransmitReceive(lora->hspi, tx, rx, 2, 100);
    nss_high(lora);

    return rx[1];
}

void lora_write_reg(lora_t *lora, uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { reg | 0x80, value };

    nss_low(lora);
    HAL_SPI_Transmit(lora->hspi, tx, 2, 100);
    nss_high(lora);
}

/* ============================================================================
 * Initialisation
 * ============================================================================ */

lora_status_t lora_init(lora_t *lora, SPI_HandleTypeDef *hspi)
{
    if (!lora || !hspi) return LORA_ERR_INIT;

    lora->hspi       = hspi;
    lora->nss_port   = LORA_NSS_PORT;
    lora->nss_pin    = LORA_NSS_PIN;
    lora->reset_port = LORA_RESET_PORT;
    lora->reset_pin  = LORA_RESET_PIN;
    lora->dio0_port  = LORA_DIO0_PORT;
    lora->dio0_pin   = LORA_DIO0_PIN;
    lora->tx_done    = false;
    lora->last_rssi  = 0;
    lora->last_snr   = 0;

    /* Hardware reset */
    HAL_GPIO_WritePin(lora->reset_port, lora->reset_pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(lora->reset_port, lora->reset_pin, GPIO_PIN_SET);
    HAL_Delay(10);

    /* Verify chip — read version register (should be 0x12 for SX1276) */
    uint8_t version = lora_read_reg(lora, SX1276_REG_VERSION);
    if (version != 0x12) {
        return LORA_ERR_NO_DEVICE;
    }

    /* Enter sleep mode to configure */
    lora_write_reg(lora, SX1276_REG_OP_MODE,
                   SX1276_MODE_SLEEP | SX1276_MODE_LORA);
    HAL_Delay(10);

    /* Set frequency */
    uint64_t frf = ((uint64_t)LORA_FREQUENCY_HZ << 19) / 32000000ULL;
    lora_write_reg(lora, SX1276_REG_FR_MSB, (uint8_t)(frf >> 16));
    lora_write_reg(lora, SX1276_REG_FR_MID, (uint8_t)(frf >> 8));
    lora_write_reg(lora, SX1276_REG_FR_LSB, (uint8_t)(frf));

    /* Set TX power */
    if (LORA_TX_POWER_DBM > 17) {
        /* Use PA_BOOST with high power settings */
        lora_write_reg(lora, SX1276_REG_PA_DAC, 0x87);  /* Enable +20 dBm */
        lora_write_reg(lora, SX1276_REG_PA_CONFIG,
                       0x80 | (LORA_TX_POWER_DBM - 5));
    } else {
        lora_write_reg(lora, SX1276_REG_PA_DAC, 0x84);  /* Default */
        lora_write_reg(lora, SX1276_REG_PA_CONFIG,
                       0x80 | (LORA_TX_POWER_DBM - 2));
    }

    /* Set OCP (over-current protection) */
    lora_write_reg(lora, SX1276_REG_OCP, 0x2B);  /* 100 mA */

    /* Set LNA gain to max */
    lora_write_reg(lora, SX1276_REG_LNA, 0x23);

    /* Configure modem */
    /* ModemConfig1: BW | CodingRate | ImplicitHeader */
    uint8_t mc1 = 0;
    /* Bandwidth: 125 kHz = 0x70 */
    mc1 |= 0x70;
    /* Coding rate 4/5 = 0x02 */
    mc1 |= 0x02;
    /* Explicit header mode = 0x00 */
    lora_write_reg(lora, SX1276_REG_MODEM_CONFIG_1, mc1);

    /* ModemConfig2: SF | TxContinuousMode | CRC */
    uint8_t mc2 = 0;
    mc2 |= (LORA_SPREADING_FACTOR << 4);
    if (LORA_CRC_ENABLED) mc2 |= 0x04;
    lora_write_reg(lora, SX1276_REG_MODEM_CONFIG_2, mc2);

    /* ModemConfig3: LowDataRateOptimize | AGC auto */
    uint8_t mc3 = 0x04;  /* AGC auto on */
    /* Enable LDR optimisation for SF11/SF12 with BW125 */
    if (LORA_SPREADING_FACTOR >= 11) mc3 |= 0x08;
    lora_write_reg(lora, SX1276_REG_MODEM_CONFIG_3, mc3);

    /* Preamble length */
    lora_write_reg(lora, SX1276_REG_PREAMBLE_MSB,
                   (uint8_t)(LORA_PREAMBLE_LENGTH >> 8));
    lora_write_reg(lora, SX1276_REG_PREAMBLE_LSB,
                   (uint8_t)(LORA_PREAMBLE_LENGTH));

    /* Sync word */
    lora_write_reg(lora, 0x39, LORA_SYNC_WORD);

    /* DIO0 mapping: TxDone on DIO0 */
    lora_write_reg(lora, SX1276_REG_DIO_MAPPING_1, 0x40);

    /* Set FIFO base addresses */
    lora_write_reg(lora, SX1276_REG_FIFO_TX_BASE, 0x00);
    lora_write_reg(lora, SX1276_REG_FIFO_RX_BASE, 0x00);

    /* Go to standby */
    lora_write_reg(lora, SX1276_REG_OP_MODE,
                   SX1276_MODE_STANDBY | SX1276_MODE_LORA);

    lora->initialised = true;
    return LORA_OK;
}

/* ============================================================================
 * Transmit
 * ============================================================================ */

lora_status_t lora_transmit(lora_t *lora, const uint8_t *data, uint8_t length)
{
    if (!lora || !lora->initialised || !data) return LORA_ERR_INIT;
    if (length > LORA_MAX_PAYLOAD_SIZE) return LORA_ERR_INIT;

    /* Enter standby */
    lora_write_reg(lora, SX1276_REG_OP_MODE,
                   SX1276_MODE_STANDBY | SX1276_MODE_LORA);

    /* Set FIFO pointer to TX base */
    lora_write_reg(lora, SX1276_REG_FIFO_ADDR_PTR, 0x00);

    /* Write data to FIFO */
    for (uint8_t i = 0; i < length; i++) {
        lora_write_reg(lora, SX1276_REG_FIFO, data[i]);
    }

    /* Set payload length */
    lora_write_reg(lora, SX1276_REG_PAYLOAD_LENGTH, length);

    /* Clear IRQ flags */
    lora_write_reg(lora, SX1276_REG_IRQ_FLAGS, 0xFF);

    /* Start transmission */
    lora->tx_done = false;
    lora_write_reg(lora, SX1276_REG_OP_MODE,
                   SX1276_MODE_TX | SX1276_MODE_LORA);

    /* Wait for TX done (polling with timeout) */
    uint32_t start = HAL_GetTick();
    while (!lora->tx_done) {
        /* Check IRQ flag directly if DIO0 interrupt not working */
        uint8_t irq = lora_read_reg(lora, SX1276_REG_IRQ_FLAGS);
        if (irq & SX1276_IRQ_TX_DONE) {
            lora->tx_done = true;
            break;
        }

        if ((HAL_GetTick() - start) > LORA_TX_TIMEOUT_MS) {
            /* Clear flags and return to standby */
            lora_write_reg(lora, SX1276_REG_IRQ_FLAGS, 0xFF);
            lora_write_reg(lora, SX1276_REG_OP_MODE,
                           SX1276_MODE_STANDBY | SX1276_MODE_LORA);
            return LORA_ERR_TIMEOUT;
        }

        HAL_Delay(1);
    }

    /* Clear IRQ flags */
    lora_write_reg(lora, SX1276_REG_IRQ_FLAGS, 0xFF);

    /* Return to standby */
    lora_write_reg(lora, SX1276_REG_OP_MODE,
                   SX1276_MODE_STANDBY | SX1276_MODE_LORA);

    return LORA_OK;
}

/* ============================================================================
 * Power modes
 * ============================================================================ */

void lora_sleep(lora_t *lora)
{
    if (!lora || !lora->initialised) return;
    lora_write_reg(lora, SX1276_REG_OP_MODE,
                   SX1276_MODE_SLEEP | SX1276_MODE_LORA);
}

void lora_standby(lora_t *lora)
{
    if (!lora || !lora->initialised) return;
    lora_write_reg(lora, SX1276_REG_OP_MODE,
                   SX1276_MODE_STANDBY | SX1276_MODE_LORA);
}

/* ============================================================================
 * Interrupt handler
 * ============================================================================ */

void lora_dio0_irq_handler(lora_t *lora)
{
    if (!lora) return;
    uint8_t irq = lora_read_reg(lora, SX1276_REG_IRQ_FLAGS);

    if (irq & SX1276_IRQ_TX_DONE) {
        lora->tx_done = true;
    }

    /* Clear all IRQ flags */
    lora_write_reg(lora, SX1276_REG_IRQ_FLAGS, 0xFF);
}

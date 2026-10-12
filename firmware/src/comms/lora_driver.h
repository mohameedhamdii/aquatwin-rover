/**
 * @file lora_driver.h
 * @brief SX1276/RFM95W LoRa radio driver
 */

#ifndef LORA_DRIVER_H
#define LORA_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32l4xx_hal.h"

/**
 * @brief LoRa driver status codes
 */
typedef enum {
    LORA_OK = 0,
    LORA_ERR_INIT,
    LORA_ERR_TIMEOUT,
    LORA_ERR_CRC,
    LORA_ERR_NO_DEVICE,
    LORA_ERR_BUSY,
} lora_status_t;

/**
 * @brief LoRa driver context
 */
typedef struct {
    SPI_HandleTypeDef *hspi;        /**< SPI handle */
    GPIO_TypeDef *nss_port;         /**< NSS (chip select) port */
    uint16_t      nss_pin;          /**< NSS pin */
    GPIO_TypeDef *reset_port;       /**< Reset port */
    uint16_t      reset_pin;        /**< Reset pin */
    GPIO_TypeDef *dio0_port;        /**< DIO0 (interrupt) port */
    uint16_t      dio0_pin;         /**< DIO0 pin */
    bool          tx_done;          /**< Set by ISR on TX complete */
    bool          initialised;
    int16_t       last_rssi;        /**< RSSI of last received packet */
    int8_t        last_snr;         /**< SNR of last received packet */
} lora_t;

/**
 * @brief Initialise the LoRa radio
 * @param lora  Pointer to LoRa context
 * @param hspi  SPI handle (must be pre-initialised)
 * @return LORA_OK on success
 */
lora_status_t lora_init(lora_t *lora, SPI_HandleTypeDef *hspi);

/**
 * @brief Transmit a data packet
 * @param lora      Pointer to LoRa context
 * @param data      Pointer to data buffer
 * @param length    Length of data (max LORA_MAX_PAYLOAD_SIZE)
 * @return LORA_OK on success
 */
lora_status_t lora_transmit(lora_t *lora, const uint8_t *data, uint8_t length);

/**
 * @brief Put the radio into sleep mode (lowest power)
 * @param lora  Pointer to LoRa context
 */
void lora_sleep(lora_t *lora);

/**
 * @brief Wake the radio from sleep to standby
 * @param lora  Pointer to LoRa context
 */
void lora_standby(lora_t *lora);

/**
 * @brief DIO0 interrupt handler — call from EXTI ISR
 * @param lora  Pointer to LoRa context
 */
void lora_dio0_irq_handler(lora_t *lora);

/**
 * @brief Read a register from the SX1276
 * @param lora  Pointer to LoRa context
 * @param reg   Register address
 * @return Register value
 */
uint8_t lora_read_reg(lora_t *lora, uint8_t reg);

/**
 * @brief Write a register to the SX1276
 * @param lora  Pointer to LoRa context
 * @param reg   Register address
 * @param value Value to write
 */
void lora_write_reg(lora_t *lora, uint8_t reg, uint8_t value);

#endif /* LORA_DRIVER_H */

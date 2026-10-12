/**
 * @file satellite.h
 * @brief Optional Iridium SBD satellite backhaul driver (RockBLOCK 9603)
 */

#ifndef SATELLITE_H
#define SATELLITE_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32l4xx_hal.h"

typedef enum {
    SAT_OK = 0,
    SAT_ERR_INIT,
    SAT_ERR_TIMEOUT,
    SAT_ERR_NO_SIGNAL,
    SAT_ERR_SEND_FAIL,
} sat_status_t;

typedef struct {
    UART_HandleTypeDef *huart;
    bool initialised;
    uint8_t signal_quality;
} satellite_t;

sat_status_t satellite_init(satellite_t *sat, UART_HandleTypeDef *huart);
sat_status_t satellite_send(satellite_t *sat, const uint8_t *data, uint8_t length);
uint8_t      satellite_get_signal(satellite_t *sat);
void         satellite_sleep(satellite_t *sat);

#endif /* SATELLITE_H */

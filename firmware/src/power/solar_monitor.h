/**
 * @file solar_monitor.h
 * @brief Solar panel voltage monitoring
 */

#ifndef SOLAR_MONITOR_H
#define SOLAR_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32l4xx_hal.h"

typedef struct {
    ADC_HandleTypeDef *hadc;
    uint16_t voltage_mv;
    uint8_t  power_pct;     /**< Percentage of max panel output */
    bool     is_charging;   /**< True if panel voltage above charging threshold */
    bool     initialised;
} solar_monitor_t;

int      solar_monitor_init(solar_monitor_t *sm, ADC_HandleTypeDef *hadc);
uint16_t solar_monitor_read_mv(solar_monitor_t *sm);
uint8_t  solar_monitor_get_power_pct(solar_monitor_t *sm);
bool     solar_monitor_is_charging(const solar_monitor_t *sm);

#endif /* SOLAR_MONITOR_H */

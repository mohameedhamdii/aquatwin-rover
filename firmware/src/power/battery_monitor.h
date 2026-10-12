/**
 * @file battery_monitor.h
 * @brief LiPo battery voltage monitoring and state-of-charge estimation
 */

#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32l4xx_hal.h"

typedef struct {
    ADC_HandleTypeDef *hadc;
    uint16_t voltage_mv;        /**< Current battery voltage (mV) */
    uint8_t  soc_pct;           /**< State of charge (0-100%) */
    bool     is_low;            /**< True if below low threshold */
    bool     is_critical;       /**< True if below critical threshold */
    bool     initialised;
} battery_monitor_t;

int      battery_monitor_init(battery_monitor_t *bm, ADC_HandleTypeDef *hadc);
uint16_t battery_monitor_read_mv(battery_monitor_t *bm);
uint8_t  battery_monitor_get_soc(battery_monitor_t *bm);
bool     battery_monitor_is_low(const battery_monitor_t *bm);
bool     battery_monitor_is_critical(const battery_monitor_t *bm);

#endif /* BATTERY_MONITOR_H */

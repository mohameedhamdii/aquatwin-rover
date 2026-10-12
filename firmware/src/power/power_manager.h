/**
 * @file power_manager.h
 * @brief System power management — sleep modes and adaptive TX rate
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "power/battery_monitor.h"
#include "power/solar_monitor.h"

typedef enum {
    POWER_MODE_NORMAL,      /**< Full operation, 60s TX interval */
    POWER_MODE_LOW_POWER,   /**< Reduced TX rate (300s interval) */
    POWER_MODE_CRITICAL,    /**< Deep sleep, wake only on solar */
} power_mode_t;

typedef struct {
    battery_monitor_t battery;
    solar_monitor_t   solar;
    power_mode_t      mode;
    uint32_t          sleep_duration_s;
    bool              initialised;
} power_manager_t;

int           power_manager_init(power_manager_t *pm, ADC_HandleTypeDef *hadc);
void          power_manager_update(power_manager_t *pm);
uint32_t      power_manager_get_sleep_duration(const power_manager_t *pm);
power_mode_t  power_manager_get_mode(const power_manager_t *pm);
void          power_manager_enter_stop_mode(uint32_t seconds);
void          power_manager_enter_deep_sleep(void);

#endif /* POWER_MANAGER_H */

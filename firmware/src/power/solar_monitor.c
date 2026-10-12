/**
 * @file solar_monitor.c
 * @brief Solar panel voltage monitoring implementation
 */

#include "power/solar_monitor.h"
#include "sensors/sensor_manager.h"
#include "calibration.h"
#include "pin_map.h"

int solar_monitor_init(solar_monitor_t *sm, ADC_HandleTypeDef *hadc)
{
    if (!sm || !hadc) return -1;

    sm->hadc        = hadc;
    sm->voltage_mv  = 0;
    sm->power_pct   = 0;
    sm->is_charging = false;
    sm->initialised = true;

    return 0;
}

uint16_t solar_monitor_read_mv(solar_monitor_t *sm)
{
    if (!sm || !sm->initialised) return 0;

    uint16_t adc_raw = sensor_manager_read_adc(sm->hadc, SOLAR_ADC_CHANNEL);
    float adc_mv = (float)ADC_TO_MV(adc_raw);

    sm->voltage_mv = (uint16_t)(adc_mv * SOLAR_DIVIDER_RATIO);

    /* Compute power percentage relative to max panel voltage */
    if (sm->voltage_mv >= SOLAR_PANEL_MAX_MV) {
        sm->power_pct = 100;
    } else {
        sm->power_pct = (uint8_t)((uint32_t)sm->voltage_mv * 100 / SOLAR_PANEL_MAX_MV);
    }

    sm->is_charging = (sm->voltage_mv >= SOLAR_CHARGING_MIN_MV);

    return sm->voltage_mv;
}

uint8_t solar_monitor_get_power_pct(solar_monitor_t *sm)
{
    if (!sm || !sm->initialised) return 0;
    return sm->power_pct;
}

bool solar_monitor_is_charging(const solar_monitor_t *sm)
{
    if (!sm) return false;
    return sm->is_charging;
}

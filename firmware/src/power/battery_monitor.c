/**
 * @file battery_monitor.c
 * @brief LiPo battery monitoring implementation
 *
 * Reads battery voltage via a voltage divider on an ADC channel,
 * maps to state-of-charge using a LiPo discharge curve lookup.
 */

#include "power/battery_monitor.h"
#include "sensors/sensor_manager.h"
#include "calibration.h"
#include "pin_map.h"

/**
 * @brief LiPo discharge curve (voltage in mV → SoC percentage)
 * Sampled at 10% intervals from a typical single-cell LiPo.
 */
static const struct {
    uint16_t voltage_mv;
    uint8_t  soc_pct;
} lipo_curve[] = {
    { 4200, 100 },
    { 4100,  90 },
    { 3980,  80 },
    { 3870,  70 },
    { 3790,  60 },
    { 3730,  50 },
    { 3700,  40 },
    { 3670,  30 },
    { 3610,  20 },
    { 3500,  10 },
    { 3300,   5 },
    { 3200,   0 },
};

#define LIPO_CURVE_SIZE  (sizeof(lipo_curve) / sizeof(lipo_curve[0]))

static uint8_t voltage_to_soc(uint16_t mv)
{
    if (mv >= lipo_curve[0].voltage_mv) return 100;
    if (mv <= lipo_curve[LIPO_CURVE_SIZE - 1].voltage_mv) return 0;

    for (int i = 0; i < (int)(LIPO_CURVE_SIZE - 1); i++) {
        if (mv >= lipo_curve[i + 1].voltage_mv) {
            /* Linear interpolation between table entries */
            float frac = (float)(mv - lipo_curve[i + 1].voltage_mv) /
                         (float)(lipo_curve[i].voltage_mv - lipo_curve[i + 1].voltage_mv);
            return (uint8_t)(lipo_curve[i + 1].soc_pct +
                   frac * (lipo_curve[i].soc_pct - lipo_curve[i + 1].soc_pct));
        }
    }

    return 0;
}

int battery_monitor_init(battery_monitor_t *bm, ADC_HandleTypeDef *hadc)
{
    if (!bm || !hadc) return -1;

    bm->hadc        = hadc;
    bm->voltage_mv  = 0;
    bm->soc_pct     = 0;
    bm->is_low      = false;
    bm->is_critical = false;
    bm->initialised = true;

    return 0;
}

uint16_t battery_monitor_read_mv(battery_monitor_t *bm)
{
    if (!bm || !bm->initialised) return 0;

    uint16_t adc_raw = sensor_manager_read_adc(bm->hadc, BATTERY_ADC_CHANNEL);
    float adc_mv = (float)ADC_TO_MV(adc_raw);

    /* Apply voltage divider ratio */
    bm->voltage_mv = (uint16_t)(adc_mv * BATTERY_DIVIDER_RATIO);

    /* Update SoC */
    bm->soc_pct = voltage_to_soc(bm->voltage_mv);

    /* Update thresholds */
    bm->is_low      = (bm->soc_pct <= BATTERY_LOW_THRESHOLD_PCT);
    bm->is_critical = (bm->soc_pct <= BATTERY_CRITICAL_PCT);

    return bm->voltage_mv;
}

uint8_t battery_monitor_get_soc(battery_monitor_t *bm)
{
    if (!bm || !bm->initialised) return 0;
    return bm->soc_pct;
}

bool battery_monitor_is_low(const battery_monitor_t *bm)
{
    if (!bm) return false;
    return bm->is_low;
}

bool battery_monitor_is_critical(const battery_monitor_t *bm)
{
    if (!bm) return false;
    return bm->is_critical;
}

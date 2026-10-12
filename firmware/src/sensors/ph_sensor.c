/**
 * @file ph_sensor.c
 * @brief pH sensor driver implementation
 *
 * Reads DFRobot Gravity Analog pH Sensor V2 via ADC, applies two-point
 * calibration, and compensates for temperature using the Nernst equation.
 */

#include "sensors/ph_sensor.h"
#include "calibration.h"
#include <math.h>

/* ============================================================================
 * Private helpers
 * ============================================================================ */

/**
 * @brief Compute Nernst temperature compensation factor
 *
 * The Nernst equation gives the theoretical electrode potential:
 *   E = E0 + (RT/nF) * ln(aH+)
 * Temperature affects the slope of mV-per-pH-unit.
 * At 25°C the ideal slope is 59.16 mV/pH.
 * At temperature T: slope_T = slope_25 * (273.15 + T) / (273.15 + 25)
 *
 * @param cal_temp_c    Temperature at calibration (°C)
 * @param current_temp_c Current water temperature (°C)
 * @return Compensation factor (multiply with slope)
 */
static float nernst_compensation(float cal_temp_c, float current_temp_c)
{
    const float cal_kelvin = 273.15f + cal_temp_c;
    const float cur_kelvin = 273.15f + current_temp_c;

    if (cal_kelvin < 1.0f) {
        return 1.0f;  /* Avoid division by zero */
    }

    return cur_kelvin / cal_kelvin;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int ph_sensor_init(ph_sensor_t *ctx)
{
    if (!ctx) {
        return -1;
    }

    /* Load default calibration from calibration.h */
    ctx->cal.v_neutral_mv = (float)PH_CAL_V_NEUTRAL_MV;
    ctx->cal.v_acid_mv    = (float)PH_CAL_V_ACID_MV;
    ctx->cal.cal_temp_c   = PH_CAL_REFERENCE_TEMP_C;

    /* Compute slope: ΔpH / ΔV */
    float dv = ctx->cal.v_neutral_mv - ctx->cal.v_acid_mv;
    if (fabsf(dv) < 1.0f) {
        /* Calibration values are too close — invalid */
        ctx->cal.slope = 0.0f;
        ctx->initialised = false;
        return -2;
    }
    ctx->cal.slope = (PH_CAL_NEUTRAL_PH - PH_CAL_ACID_PH) / dv;

    /* Initialise runtime state */
    ctx->adc_accum    = 0;
    ctx->sample_count = 0;
    ctx->last_ph      = 7.0f;
    ctx->initialised  = true;

    return 0;
}

void ph_sensor_accumulate(ph_sensor_t *ctx, uint16_t adc_raw)
{
    if (!ctx || !ctx->initialised) {
        return;
    }

    ctx->adc_accum += adc_raw;
    ctx->sample_count++;
}

float ph_sensor_compute(ph_sensor_t *ctx, float temp_c)
{
    if (!ctx || !ctx->initialised || ctx->sample_count == 0) {
        return -1.0f;
    }

    /* Compute average ADC value */
    uint32_t avg_adc = ctx->adc_accum / ctx->sample_count;

    /* Convert to millivolts */
    float voltage_mv = (float)ADC_TO_MV(avg_adc);

    /* Apply Nernst temperature compensation to the slope */
    float comp = nernst_compensation(ctx->cal.cal_temp_c, temp_c);
    float adjusted_slope = ctx->cal.slope * comp;

    /* Compute pH: pH = 7.0 + (V_measured - V_neutral) * slope */
    float ph = PH_CAL_NEUTRAL_PH + (voltage_mv - ctx->cal.v_neutral_mv) * adjusted_slope;

    /* Clamp to valid range */
    if (ph < PH_VALID_MIN) ph = PH_VALID_MIN;
    if (ph > PH_VALID_MAX) ph = PH_VALID_MAX;

    /* Store and reset */
    ctx->last_ph = ph;
    ctx->adc_accum = 0;
    ctx->sample_count = 0;

    return ph;
}

float ph_sensor_get_last(const ph_sensor_t *ctx)
{
    if (!ctx || !ctx->initialised) {
        return -1.0f;
    }
    return ctx->last_ph;
}

void ph_sensor_set_calibration(ph_sensor_t *ctx, float v_neutral_mv,
                                float v_acid_mv, float cal_temp_c)
{
    if (!ctx) {
        return;
    }

    ctx->cal.v_neutral_mv = v_neutral_mv;
    ctx->cal.v_acid_mv    = v_acid_mv;
    ctx->cal.cal_temp_c   = cal_temp_c;

    /* Recompute slope */
    float dv = v_neutral_mv - v_acid_mv;
    if (fabsf(dv) < 1.0f) {
        ctx->cal.slope = 0.0f;
    } else {
        ctx->cal.slope = (PH_CAL_NEUTRAL_PH - PH_CAL_ACID_PH) / dv;
    }
}

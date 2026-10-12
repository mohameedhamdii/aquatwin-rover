/**
 * @file turbidity_sensor.c
 * @brief Turbidity sensor driver implementation
 *
 * Reads DFRobot SEN0189 analog turbidity sensor via ADC.
 * Applies a polynomial curve fit to convert voltage to NTU.
 */

#include "sensors/turbidity_sensor.h"
#include "calibration.h"

int turb_sensor_init(turb_sensor_t *ctx)
{
    if (!ctx) return -1;

    ctx->cal.poly_a2 = TURB_POLY_A2;
    ctx->cal.poly_a1 = TURB_POLY_A1;
    ctx->cal.poly_a0 = TURB_POLY_A0;

    ctx->adc_accum    = 0;
    ctx->sample_count = 0;
    ctx->last_ntu     = 0.0f;
    ctx->initialised  = true;

    return 0;
}

void turb_sensor_accumulate(turb_sensor_t *ctx, uint16_t adc_raw)
{
    if (!ctx || !ctx->initialised) return;
    ctx->adc_accum += adc_raw;
    ctx->sample_count++;
}

float turb_sensor_compute(turb_sensor_t *ctx)
{
    if (!ctx || !ctx->initialised || ctx->sample_count == 0) return -1.0f;

    /* Average ADC */
    uint32_t avg_adc = ctx->adc_accum / ctx->sample_count;

    /* Convert to volts (the sensor outputs 0–4.5V, but through a divider to 3.3V range) */
    float voltage_v = (float)ADC_TO_MV(avg_adc) / 1000.0f;

    /* Polynomial conversion: NTU = a2*V^2 + a1*V + a0 */
    float ntu = ctx->cal.poly_a2 * voltage_v * voltage_v
              + ctx->cal.poly_a1 * voltage_v
              + ctx->cal.poly_a0;

    /* Clamp */
    if (ntu < TURB_VALID_MIN) ntu = TURB_VALID_MIN;
    if (ntu > TURB_VALID_MAX) ntu = TURB_VALID_MAX;

    ctx->last_ntu = ntu;
    ctx->adc_accum = 0;
    ctx->sample_count = 0;

    return ntu;
}

float turb_sensor_get_last(const turb_sensor_t *ctx)
{
    if (!ctx || !ctx->initialised) return -1.0f;
    return ctx->last_ntu;
}

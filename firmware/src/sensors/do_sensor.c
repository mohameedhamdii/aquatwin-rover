/**
 * @file do_sensor.c
 * @brief Dissolved Oxygen sensor driver implementation
 *
 * Single-point calibrated DO measurement with temperature-dependent
 * saturation correction using a lookup table from Standard Methods.
 */

#include "sensors/do_sensor.h"
#include "calibration.h"

/* ============================================================================
 * DO Saturation Lookup Table (mg/L at 1 atm, per °C, from 0°C to 40°C)
 * Source: Standard Methods for the Examination of Water and Wastewater
 * ============================================================================ */
static const float do_sat_table[DO_SAT_TABLE_SIZE] = {
    14.62f, /* 0°C  */  14.22f, /* 1°C  */  13.83f, /* 2°C  */  13.46f, /* 3°C  */
    13.11f, /* 4°C  */  12.77f, /* 5°C  */  12.45f, /* 6°C  */  12.14f, /* 7°C  */
    11.84f, /* 8°C  */  11.56f, /* 9°C  */  11.29f, /* 10°C */  11.03f, /* 11°C */
    10.78f, /* 12°C */  10.54f, /* 13°C */  10.31f, /* 14°C */  10.08f, /* 15°C */
     9.87f, /* 16°C */   9.67f, /* 17°C */   9.47f, /* 18°C */   9.28f, /* 19°C */
     9.09f, /* 20°C */   8.92f, /* 21°C */   8.74f, /* 22°C */   8.58f, /* 23°C */
     8.42f, /* 24°C */   8.26f, /* 25°C */   8.11f, /* 26°C */   7.97f, /* 27°C */
     7.83f, /* 28°C */   7.69f, /* 29°C */   7.56f, /* 30°C */   7.43f, /* 31°C */
     7.31f, /* 32°C */   7.18f, /* 33°C */   7.07f, /* 34°C */   6.95f, /* 35°C */
     6.84f, /* 36°C */   6.73f, /* 37°C */   6.62f, /* 38°C */   6.51f, /* 39°C */
     6.41f  /* 40°C */
};

float do_saturation_at_temp(float temp_c)
{
    if (temp_c < 0.0f)  temp_c = 0.0f;
    if (temp_c > 40.0f) temp_c = 40.0f;

    /* Linear interpolation between table entries */
    int idx_low = (int)temp_c;
    int idx_high = idx_low + 1;
    if (idx_high >= DO_SAT_TABLE_SIZE) idx_high = DO_SAT_TABLE_SIZE - 1;

    float frac = temp_c - (float)idx_low;
    return do_sat_table[idx_low] + frac * (do_sat_table[idx_high] - do_sat_table[idx_low]);
}

int do_sensor_init(do_sensor_t *ctx)
{
    if (!ctx) return -1;

    ctx->cal.v_reference_mv     = (float)DO_CAL_V_REFERENCE_MV;
    ctx->cal.ref_temp_c         = DO_CAL_REFERENCE_TEMP_C;
    ctx->cal.ref_saturation_mg_l = DO_CAL_SATURATION_MG_L;

    ctx->adc_accum    = 0;
    ctx->sample_count = 0;
    ctx->last_do_mg_l = 0.0f;
    ctx->initialised  = true;

    return 0;
}

void do_sensor_accumulate(do_sensor_t *ctx, uint16_t adc_raw)
{
    if (!ctx || !ctx->initialised) return;
    ctx->adc_accum += adc_raw;
    ctx->sample_count++;
}

float do_sensor_compute(do_sensor_t *ctx, float temp_c)
{
    if (!ctx || !ctx->initialised || ctx->sample_count == 0) return -1.0f;

    /* Average ADC */
    uint32_t avg_adc = ctx->adc_accum / ctx->sample_count;
    float voltage_mv = (float)ADC_TO_MV(avg_adc);

    /* 
     * DO = (V_measured / V_reference) * saturation_DO(current_temp)
     * The ratio compensates for probe aging/drift since calibration.
     */
    float sat_do = do_saturation_at_temp(temp_c);
    float do_mg_l = 0.0f;

    if (ctx->cal.v_reference_mv > 1.0f) {
        do_mg_l = (voltage_mv / ctx->cal.v_reference_mv) * sat_do;
    }

    /* Clamp */
    if (do_mg_l < DO_VALID_MIN) do_mg_l = DO_VALID_MIN;
    if (do_mg_l > DO_VALID_MAX) do_mg_l = DO_VALID_MAX;

    ctx->last_do_mg_l = do_mg_l;
    ctx->adc_accum = 0;
    ctx->sample_count = 0;

    return do_mg_l;
}

float do_sensor_get_last(const do_sensor_t *ctx)
{
    if (!ctx || !ctx->initialised) return -1.0f;
    return ctx->last_do_mg_l;
}

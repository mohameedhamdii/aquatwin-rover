/**
 * @file do_sensor.h
 * @brief Dissolved Oxygen sensor driver for DFRobot SEN0237-A
 */

#ifndef DO_SENSOR_H
#define DO_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief DO sensor calibration data
 */
typedef struct {
    float v_reference_mv;       /**< ADC voltage in air-saturated water (mV) */
    float ref_temp_c;           /**< Temperature during calibration */
    float ref_saturation_mg_l;  /**< DO saturation at calibration temp */
} do_cal_t;

/**
 * @brief DO sensor context
 */
typedef struct {
    do_cal_t cal;
    uint32_t adc_accum;
    uint16_t sample_count;
    float last_do_mg_l;
    bool initialised;
} do_sensor_t;

int   do_sensor_init(do_sensor_t *ctx);
void  do_sensor_accumulate(do_sensor_t *ctx, uint16_t adc_raw);
float do_sensor_compute(do_sensor_t *ctx, float temp_c);
float do_sensor_get_last(const do_sensor_t *ctx);

/**
 * @brief Get saturated DO value at a given temperature (mg/L at 1 atm)
 * @param temp_c  Water temperature in °C
 * @return Saturated DO in mg/L
 */
float do_saturation_at_temp(float temp_c);

#endif /* DO_SENSOR_H */

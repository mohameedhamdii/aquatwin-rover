/**
 * @file ph_sensor.h
 * @brief pH sensor driver for DFRobot Gravity Analog pH Sensor V2 (SEN0161-V2)
 *
 * Two-point calibrated pH measurement with temperature compensation
 * via the Nernst equation.
 */

#ifndef PH_SENSOR_H
#define PH_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief pH sensor calibration data
 */
typedef struct {
    float v_neutral_mv;     /**< ADC voltage at pH 7.0 buffer (mV) */
    float v_acid_mv;        /**< ADC voltage at pH 4.0 buffer (mV) */
    float slope;            /**< Computed calibration slope */
    float cal_temp_c;       /**< Temperature at calibration time (°C) */
} ph_cal_t;

/**
 * @brief pH sensor context
 */
typedef struct {
    ph_cal_t cal;           /**< Calibration data */
    uint32_t adc_accum;     /**< Accumulator for oversampling */
    uint16_t sample_count;  /**< Current sample count in averaging window */
    float last_ph;          /**< Last computed pH value */
    bool initialised;       /**< True if initialised */
} ph_sensor_t;

/**
 * @brief Initialise the pH sensor driver with calibration constants
 * @param ctx   Pointer to pH sensor context
 * @return 0 on success, negative on error
 */
int ph_sensor_init(ph_sensor_t *ctx);

/**
 * @brief Accumulate one ADC sample (call at 10 Hz)
 * @param ctx       Pointer to pH sensor context
 * @param adc_raw   Raw 12-bit ADC value
 */
void ph_sensor_accumulate(ph_sensor_t *ctx, uint16_t adc_raw);

/**
 * @brief Compute pH from accumulated samples (call at 1 Hz after 10 accumulations)
 * @param ctx       Pointer to pH sensor context
 * @param temp_c    Current water temperature for Nernst compensation
 * @return pH value (0.0–14.0), or -1.0 on error
 */
float ph_sensor_compute(ph_sensor_t *ctx, float temp_c);

/**
 * @brief Get the last computed pH value
 * @param ctx   Pointer to pH sensor context
 * @return Last pH reading
 */
float ph_sensor_get_last(const ph_sensor_t *ctx);

/**
 * @brief Update calibration values at runtime
 * @param ctx           Pointer to pH sensor context
 * @param v_neutral_mv  Voltage at pH 7.0 (mV)
 * @param v_acid_mv     Voltage at pH 4.0 (mV)
 * @param cal_temp_c    Calibration temperature (°C)
 */
void ph_sensor_set_calibration(ph_sensor_t *ctx, float v_neutral_mv,
                                float v_acid_mv, float cal_temp_c);

#endif /* PH_SENSOR_H */

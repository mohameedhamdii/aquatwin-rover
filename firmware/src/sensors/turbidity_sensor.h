/**
 * @file turbidity_sensor.h
 * @brief Turbidity sensor driver for DFRobot Gravity Analog Turbidity Sensor (SEN0189)
 */

#ifndef TURBIDITY_SENSOR_H
#define TURBIDITY_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Turbidity calibration data (polynomial fit)
 */
typedef struct {
    float poly_a2;      /**< Quadratic coefficient */
    float poly_a1;      /**< Linear coefficient */
    float poly_a0;      /**< Constant term */
} turb_cal_t;

/**
 * @brief Turbidity sensor context
 */
typedef struct {
    turb_cal_t cal;
    uint32_t adc_accum;
    uint16_t sample_count;
    float last_ntu;
    bool initialised;
} turb_sensor_t;

int  turb_sensor_init(turb_sensor_t *ctx);
void turb_sensor_accumulate(turb_sensor_t *ctx, uint16_t adc_raw);
float turb_sensor_compute(turb_sensor_t *ctx);
float turb_sensor_get_last(const turb_sensor_t *ctx);

#endif /* TURBIDITY_SENSOR_H */

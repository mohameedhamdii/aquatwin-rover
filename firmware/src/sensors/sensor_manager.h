/**
 * @file sensor_manager.h
 * @brief Coordinates all water quality sensor readings
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "sensors/ph_sensor.h"
#include "sensors/turbidity_sensor.h"
#include "sensors/do_sensor.h"
#include "sensors/temperature_sensor.h"

/**
 * @brief Raw sensor data (ADC values)
 */
typedef struct {
    uint16_t ph_adc;
    uint16_t turb_adc;
    uint16_t do_adc;
    float    temp_c;        /* Already in °C from DS18B20 */
} sensor_raw_t;

/**
 * @brief Calibrated sensor data (physical units)
 */
typedef struct {
    float ph;               /* pH units (0–14) */
    float turbidity;        /* NTU */
    float do_mg_l;          /* mg/L dissolved oxygen */
    float temp_c;           /* °C */
    bool  valid;            /* True if all readings are valid */
    uint32_t timestamp_ms;  /* System tick when data was captured */
} sensor_data_t;

/**
 * @brief Sensor manager context (holds all sensor driver contexts)
 */
typedef struct {
    ph_sensor_t    ph;
    turb_sensor_t  turb;
    do_sensor_t    dox;     /* 'do' is a C keyword, use 'dox' */
    temp_sensor_t  temp;
    ADC_HandleTypeDef *hadc;
    bool initialised;
} sensor_manager_t;

/**
 * @brief Initialise all sensors and ADC
 * @param mgr   Pointer to sensor manager context
 * @param hadc  Pointer to HAL ADC handle (configured for multi-channel scanning)
 * @return 0 on success, negative on error
 */
int sensor_manager_init(sensor_manager_t *mgr, ADC_HandleTypeDef *hadc);

/**
 * @brief Read all sensors (blocking, takes ~100 ms for oversampling)
 * @param mgr  Pointer to sensor manager
 * @return Calibrated sensor data
 */
sensor_data_t sensor_manager_read_all(sensor_manager_t *mgr);

/**
 * @brief Read a single ADC channel
 * @param hadc     ADC handle
 * @param channel  ADC channel to read
 * @return Raw 12-bit ADC value
 */
uint16_t sensor_manager_read_adc(ADC_HandleTypeDef *hadc, uint32_t channel);

#endif /* SENSOR_MANAGER_H */

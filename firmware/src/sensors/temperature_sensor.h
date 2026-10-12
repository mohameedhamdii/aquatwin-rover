/**
 * @file temperature_sensor.h
 * @brief DS18B20 waterproof temperature sensor driver (OneWire protocol)
 */

#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Temperature sensor context
 */
typedef struct {
    float last_temp_c;
    bool  initialised;
    bool  conversion_started;
} temp_sensor_t;

/**
 * @brief Initialise the DS18B20 driver
 * @param ctx  Pointer to temperature sensor context
 * @return 0 on success, -1 if no device found on the bus
 */
int temp_sensor_init(temp_sensor_t *ctx);

/**
 * @brief Start a temperature conversion (takes ~750 ms for 12-bit)
 * @param ctx  Pointer to temperature sensor context
 * @return 0 on success
 */
int temp_sensor_start_conversion(temp_sensor_t *ctx);

/**
 * @brief Read the temperature result after conversion completes
 * @param ctx  Pointer to temperature sensor context
 * @return Temperature in °C, or -999.0 on error
 */
float temp_sensor_read(temp_sensor_t *ctx);

/**
 * @brief Get the last read temperature
 * @param ctx  Pointer to temperature sensor context
 * @return Last temperature in °C
 */
float temp_sensor_get_last(const temp_sensor_t *ctx);

#endif /* TEMPERATURE_SENSOR_H */

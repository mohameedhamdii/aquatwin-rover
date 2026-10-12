/**
 * @file gps_driver.h
 * @brief u-blox NEO-M8N GPS module driver (NMEA parsing)
 */

#ifndef GPS_DRIVER_H
#define GPS_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32l4xx_hal.h"
#include "comms/packet.h"  /* for gps_fix_t */

#define GPS_NMEA_BUF_SIZE   128

typedef struct {
    UART_HandleTypeDef *huart;
    gps_fix_t fix;
    float     heading_deg;      /**< Course over ground (degrees) */
    float     speed_knots;      /**< Speed over ground */
    uint8_t   num_satellites;
    char      nmea_buf[GPS_NMEA_BUF_SIZE];
    uint16_t  nmea_idx;
    bool      sentence_ready;
    bool      initialised;
} gps_driver_t;

int       gps_init(gps_driver_t *gps, UART_HandleTypeDef *huart);
void      gps_process(gps_driver_t *gps);
gps_fix_t gps_get_fix(const gps_driver_t *gps);
float     gps_get_heading(const gps_driver_t *gps);
void      gps_uart_rx_callback(gps_driver_t *gps, uint8_t byte);

#endif /* GPS_DRIVER_H */

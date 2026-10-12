/**
 * @file gps_driver.c
 * @brief NMEA GPS driver implementation
 *
 * Parses GGA and RMC sentences from u-blox NEO-M8N via UART.
 * Converts NMEA coordinates to integer format (× 1e7) for packet use.
 */

#include "nav/gps_driver.h"
#include "pin_map.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef BUILD_MODE_ROVER

/* ============================================================================
 * NMEA Parsing Helpers
 * ============================================================================ */

/**
 * @brief Verify NMEA checksum
 */
static bool nmea_verify_checksum(const char *sentence)
{
    const char *p = sentence;
    if (*p == '$') p++;

    uint8_t checksum = 0;
    while (*p && *p != '*') {
        checksum ^= (uint8_t)*p;
        p++;
    }

    if (*p != '*') return false;
    p++;

    char hex[3] = { p[0], p[1], '\0' };
    uint8_t expected = (uint8_t)strtol(hex, NULL, 16);

    return (checksum == expected);
}

/**
 * @brief Get the nth field from a comma-separated NMEA sentence
 */
static const char *nmea_get_field(const char *sentence, int field)
{
    const char *p = sentence;
    int count = 0;

    while (*p) {
        if (*p == ',') {
            count++;
            if (count == field) return p + 1;
        }
        p++;
    }

    return NULL;
}

/**
 * @brief Parse NMEA coordinate (DDMM.MMMM or DDDMM.MMMM) to degrees × 1e7
 */
static int32_t nmea_parse_coord(const char *field, const char *hemisphere)
{
    if (!field || *field == ',' || *field == '\0') return 0;

    double raw = atof(field);
    int degrees = (int)(raw / 100.0);
    double minutes = raw - (degrees * 100.0);
    double decimal_degrees = degrees + (minutes / 60.0);

    if (hemisphere && (*hemisphere == 'S' || *hemisphere == 'W')) {
        decimal_degrees = -decimal_degrees;
    }

    return (int32_t)(decimal_degrees * 1e7);
}

/**
 * @brief Parse NMEA time (HHMMSS.SS) to Unix-ish timestamp
 * Note: Without date, this is just time-of-day in seconds.
 */
static uint32_t nmea_parse_time(const char *field)
{
    if (!field || *field == ',' || *field == '\0') return 0;

    int h = (field[0] - '0') * 10 + (field[1] - '0');
    int m = (field[2] - '0') * 10 + (field[3] - '0');
    int s = (field[4] - '0') * 10 + (field[5] - '0');

    return (uint32_t)(h * 3600 + m * 60 + s);
}

/**
 * @brief Parse $GPGGA sentence (fix data)
 */
static void parse_gga(gps_driver_t *gps, const char *sentence)
{
    const char *lat_field   = nmea_get_field(sentence, 2);
    const char *lat_dir     = nmea_get_field(sentence, 3);
    const char *lon_field   = nmea_get_field(sentence, 4);
    const char *lon_dir     = nmea_get_field(sentence, 5);
    const char *fix_field   = nmea_get_field(sentence, 6);
    const char *sat_field   = nmea_get_field(sentence, 7);

    if (fix_field && *fix_field != '0' && *fix_field != ',') {
        gps->fix.fix_valid = 1;
        gps->fix.latitude  = nmea_parse_coord(lat_field, lat_dir);
        gps->fix.longitude = nmea_parse_coord(lon_field, lon_dir);
    } else {
        gps->fix.fix_valid = 0;
    }

    if (sat_field) {
        gps->num_satellites = (uint8_t)atoi(sat_field);
    }
}

/**
 * @brief Parse $GPRMC sentence (recommended minimum data)
 */
static void parse_rmc(gps_driver_t *gps, const char *sentence)
{
    const char *time_field   = nmea_get_field(sentence, 1);
    const char *status       = nmea_get_field(sentence, 2);
    const char *lat_field    = nmea_get_field(sentence, 3);
    const char *lat_dir      = nmea_get_field(sentence, 4);
    const char *lon_field    = nmea_get_field(sentence, 5);
    const char *lon_dir      = nmea_get_field(sentence, 6);
    const char *speed_field  = nmea_get_field(sentence, 7);
    const char *course_field = nmea_get_field(sentence, 8);

    if (status && *status == 'A') {
        gps->fix.fix_valid = 1;
        gps->fix.timestamp = nmea_parse_time(time_field);

        if (lat_field && lon_field) {
            gps->fix.latitude  = nmea_parse_coord(lat_field, lat_dir);
            gps->fix.longitude = nmea_parse_coord(lon_field, lon_dir);
        }

        if (speed_field && *speed_field != ',') {
            gps->speed_knots = (float)atof(speed_field);
        }

        if (course_field && *course_field != ',') {
            gps->heading_deg = (float)atof(course_field);
        }
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int gps_init(gps_driver_t *gps, UART_HandleTypeDef *huart)
{
    if (!gps || !huart) return -1;

    memset(gps, 0, sizeof(gps_driver_t));
    gps->huart = huart;
    gps->initialised = true;

    return 0;
}

void gps_uart_rx_callback(gps_driver_t *gps, uint8_t byte)
{
    if (!gps || !gps->initialised) return;

    if (byte == '$') {
        /* Start of new sentence */
        gps->nmea_idx = 0;
    }

    if (gps->nmea_idx < GPS_NMEA_BUF_SIZE - 1) {
        gps->nmea_buf[gps->nmea_idx++] = (char)byte;
        gps->nmea_buf[gps->nmea_idx] = '\0';
    }

    if (byte == '\n') {
        gps->sentence_ready = true;
    }
}

void gps_process(gps_driver_t *gps)
{
    if (!gps || !gps->sentence_ready) return;

    gps->sentence_ready = false;

    /* Verify checksum */
    if (!nmea_verify_checksum(gps->nmea_buf)) return;

    /* Route to parser based on sentence type */
    if (strncmp(gps->nmea_buf, "$GPGGA", 6) == 0 ||
        strncmp(gps->nmea_buf, "$GNGGA", 6) == 0) {
        parse_gga(gps, gps->nmea_buf);
    }
    else if (strncmp(gps->nmea_buf, "$GPRMC", 6) == 0 ||
             strncmp(gps->nmea_buf, "$GNRMC", 6) == 0) {
        parse_rmc(gps, gps->nmea_buf);
    }
}

gps_fix_t gps_get_fix(const gps_driver_t *gps)
{
    gps_fix_t empty = {0};
    if (!gps) return empty;
    return gps->fix;
}

float gps_get_heading(const gps_driver_t *gps)
{
    if (!gps) return 0.0f;
    return gps->heading_deg;
}

#endif /* BUILD_MODE_ROVER */

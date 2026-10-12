/**
 * @file packet.h
 * @brief LoRa data packet assembly and parsing
 */

#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include "sensors/sensor_manager.h"
#include "ekf/anomaly_detector.h"

/**
 * @brief Water quality data packet (32 bytes, packed)
 *
 * Binary format optimised for LoRa transmission.
 * Fixed-point encoding to avoid floating-point overhead on gateway.
 */
typedef struct __attribute__((packed)) {
    uint32_t timestamp;         /**< Unix epoch seconds (4 bytes) */
    int32_t  latitude;          /**< Latitude × 1e7 (4 bytes) */
    int32_t  longitude;         /**< Longitude × 1e7 (4 bytes) */
    int16_t  ph_x100;           /**< pH × 100 (2 bytes), e.g. 724 = 7.24 */
    int16_t  turbidity_x10;     /**< NTU × 10 (2 bytes) */
    int16_t  do_x100;           /**< mg/L × 100 (2 bytes) */
    int16_t  temp_x100;         /**< °C × 100 (2 bytes) */
    uint16_t battery_mv;        /**< Battery voltage in mV (2 bytes) */
    uint8_t  solar_power_pct;   /**< Solar panel output % (1 byte) */
    uint8_t  anomaly_flags;     /**< Bit field: pH|turb|DO|temp|NIS (1 byte) */
    uint16_t ekf_nis_x100;      /**< NIS × 100 (2 bytes) */
    uint8_t  device_id;         /**< Device identifier (1 byte) */
    uint8_t  packet_version;    /**< Packet format version (1 byte) */
    uint8_t  reserved[4];       /**< Future use (4 bytes) */
} wq_packet_t;                  /**< Total: 32 bytes */

/* Packet version */
#define WQ_PACKET_VERSION   0x01

/**
 * @brief GPS fix data for packet assembly
 */
typedef struct {
    int32_t latitude;       /**< Latitude × 1e7 */
    int32_t longitude;      /**< Longitude × 1e7 */
    uint32_t timestamp;     /**< Unix timestamp */
    uint8_t  fix_valid;     /**< 1 if GPS has valid fix */
} gps_fix_t;

/**
 * @brief Assemble a data packet from sensor readings
 * @param data          Calibrated sensor data
 * @param fix           GPS fix (can be NULL for buoy mode)
 * @param battery_mv    Battery voltage in millivolts
 * @param solar_pct     Solar panel percentage (0-100)
 * @param anomaly       Anomaly detection result
 * @return Assembled packet
 */
wq_packet_t packet_assemble(const sensor_data_t *data,
                             const gps_fix_t *fix,
                             uint16_t battery_mv,
                             uint8_t solar_pct,
                             const anomaly_result_t *anomaly);

/**
 * @brief Parse a received packet back to sensor data (for gateway/testing)
 * @param pkt   Pointer to received packet
 * @param data  Output sensor data
 * @param fix   Output GPS fix
 */
void packet_parse(const wq_packet_t *pkt, sensor_data_t *data, gps_fix_t *fix);

#endif /* PACKET_H */

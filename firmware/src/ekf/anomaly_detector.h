/**
 * @file anomaly_detector.h
 * @brief Contamination anomaly detection using EKF innovation and thresholds
 */

#ifndef ANOMALY_DETECTOR_H
#define ANOMALY_DETECTOR_H

#include <stdint.h>
#include "ekf/water_quality_ekf.h"
#include "sensors/sensor_manager.h"

/* Anomaly flag bit positions */
#define ANOMALY_FLAG_PH         (1 << 0)
#define ANOMALY_FLAG_TURB       (1 << 1)
#define ANOMALY_FLAG_DO         (1 << 2)
#define ANOMALY_FLAG_TEMP       (1 << 3)
#define ANOMALY_FLAG_NIS        (1 << 4)  /* EKF statistical anomaly */

/**
 * @brief Anomaly detection result
 */
typedef struct {
    uint8_t flags;              /**< Bitfield of anomaly flags */
    float   nis;                /**< Current NIS value */
    float   ph;                 /**< pH value that triggered (if any) */
    float   turbidity;          /**< Turbidity value (if triggered) */
    float   do_mg_l;            /**< DO value (if triggered) */
    float   temp_c;             /**< Temperature (if triggered) */
} anomaly_result_t;

/**
 * @brief Check for anomalies in the latest reading
 * @param wq    Pointer to WQ EKF (for NIS)
 * @param data  Latest calibrated sensor data
 * @return Anomaly result with flags set
 */
anomaly_result_t anomaly_detector_check(const wq_ekf_t *wq,
                                         const sensor_data_t *data);

/**
 * @brief Check if any anomaly flags are set
 * @param result  Anomaly result to check
 * @return 1 if any anomaly, 0 if normal
 */
int anomaly_is_flagged(const anomaly_result_t *result);

#endif /* ANOMALY_DETECTOR_H */

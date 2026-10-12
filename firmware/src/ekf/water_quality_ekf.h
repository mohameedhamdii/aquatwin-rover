/**
 * @file water_quality_ekf.h
 * @brief Water quality specific EKF wrapper
 *
 * Wraps the generic EKF with domain-specific state definitions
 * and tuning parameters for water quality monitoring.
 */

#ifndef WATER_QUALITY_EKF_H
#define WATER_QUALITY_EKF_H

#include "ekf/ekf.h"
#include "sensors/sensor_manager.h"

/* State indices */
#define WQ_STATE_PH         0
#define WQ_STATE_TURB       1
#define WQ_STATE_DO         2
#define WQ_STATE_TEMP       3
#define WQ_NUM_STATES       4

/**
 * @brief Water quality EKF wrapper
 */
typedef struct {
    ekf_t ekf;
    uint32_t consecutive_anomalies;  /**< Count of consecutive NIS threshold violations */
} wq_ekf_t;

/**
 * @brief Initialise the water quality EKF with default tuning
 * @param wq  Pointer to WQ EKF instance
 * @return 0 on success
 */
int wq_ekf_init(wq_ekf_t *wq);

/**
 * @brief Initialise with first measurement as initial state
 * @param wq    Pointer to WQ EKF instance
 * @param data  First sensor measurement
 * @return 0 on success
 */
int wq_ekf_init_from_measurement(wq_ekf_t *wq, const sensor_data_t *data);

/**
 * @brief Run one predict-update cycle
 * @param wq    Pointer to WQ EKF instance
 * @param data  New sensor measurement
 */
void wq_ekf_update(wq_ekf_t *wq, const sensor_data_t *data);

/**
 * @brief Get filtered (fused) water quality values
 * @param wq    Pointer to WQ EKF instance
 * @param data  Output: filtered sensor data
 */
void wq_ekf_get_filtered(const wq_ekf_t *wq, sensor_data_t *data);

/**
 * @brief Check if EKF needs reset (too many consecutive anomalies)
 * @param wq  Pointer to WQ EKF instance
 * @return true if reset needed
 */
int wq_ekf_needs_reset(const wq_ekf_t *wq);

#endif /* WATER_QUALITY_EKF_H */

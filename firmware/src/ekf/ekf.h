/**
 * @file ekf.h
 * @brief Generic Extended Kalman Filter implementation
 *
 * Configurable state dimension. Uses statically-allocated matrices
 * to avoid dynamic memory allocation on embedded systems.
 */

#ifndef EKF_H
#define EKF_H

#include <stdint.h>

/* Maximum state dimension (compile-time allocation) */
#define EKF_MAX_STATES  4

/**
 * @brief EKF instance
 */
typedef struct {
    uint8_t n;                          /**< State dimension */

    /* State estimate */
    float x[EKF_MAX_STATES];            /**< State vector */

    /* Covariance matrices */
    float P[EKF_MAX_STATES][EKF_MAX_STATES];  /**< Estimate covariance */
    float Q[EKF_MAX_STATES][EKF_MAX_STATES];  /**< Process noise covariance */
    float R[EKF_MAX_STATES][EKF_MAX_STATES];  /**< Measurement noise covariance */

    /* Intermediate computation buffers */
    float x_pred[EKF_MAX_STATES];       /**< Predicted state */
    float P_pred[EKF_MAX_STATES][EKF_MAX_STATES]; /**< Predicted covariance */
    float K[EKF_MAX_STATES][EKF_MAX_STATES];      /**< Kalman gain */
    float S[EKF_MAX_STATES][EKF_MAX_STATES];      /**< Innovation covariance */
    float y[EKF_MAX_STATES];            /**< Innovation (residual) */

    /* Last NIS (Normalised Innovation Squared) */
    float nis;

    /* Iteration counter */
    uint32_t iteration;
} ekf_t;

/**
 * @brief Initialise the EKF
 * @param ekf   Pointer to EKF instance
 * @param n     State dimension (must be <= EKF_MAX_STATES)
 * @param Q     Process noise covariance (diagonal values, length n)
 * @param R     Measurement noise covariance (diagonal values, length n)
 * @param x0    Initial state vector (length n)
 * @param P0    Initial covariance (diagonal values, length n)
 * @return 0 on success
 */
int ekf_init(ekf_t *ekf, uint8_t n, const float *Q_diag,
             const float *R_diag, const float *x0, const float *P0_diag);

/**
 * @brief EKF predict step
 *
 * For our slow-varying water quality model:
 *   x_pred = x  (identity state transition)
 *   P_pred = P + Q
 *
 * @param ekf  Pointer to EKF instance
 */
void ekf_predict(ekf_t *ekf);

/**
 * @brief EKF update step with new measurements
 *
 * Measurement model: z = H*x + v, where H = I (direct observation)
 *
 * @param ekf  Pointer to EKF instance
 * @param z    Measurement vector (length n)
 */
void ekf_update(ekf_t *ekf, const float *z);

/**
 * @brief Get the current state estimate
 * @param ekf  Pointer to EKF instance
 * @param x    Output state vector (length n)
 */
void ekf_get_state(const ekf_t *ekf, float *x);

/**
 * @brief Get the last NIS value
 * @param ekf  Pointer to EKF instance
 * @return NIS value
 */
float ekf_get_nis(const ekf_t *ekf);

/**
 * @brief Get the innovation (residual) vector
 * @param ekf  Pointer to EKF instance
 * @param y    Output innovation vector (length n)
 */
void ekf_get_innovation(const ekf_t *ekf, float *y);

/**
 * @brief Reset the EKF to new initial conditions
 * @param ekf    Pointer to EKF instance
 * @param x0     New initial state
 * @param P0_diag New initial covariance diagonal
 */
void ekf_reset(ekf_t *ekf, const float *x0, const float *P0_diag);

#endif /* EKF_H */

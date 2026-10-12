/**
 * @file ekf.c
 * @brief Generic Extended Kalman Filter implementation
 *
 * Implements predict/update for a linear-ish model (identity state transition,
 * identity measurement matrix). All matrix operations are done inline to avoid
 * pulling in a linear algebra library on the MCU.
 *
 * For the water quality application:
 *   - State: [pH, turbidity, DO, temperature]
 *   - Process model: x_k = x_{k-1} (slow-varying)
 *   - Measurement model: z_k = x_k (direct observation)
 */

#include "ekf/ekf.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * Small matrix helpers (inline, fixed-size ≤ 4×4)
 * ============================================================================ */

/**
 * @brief Invert a symmetric positive-definite matrix (up to 4×4)
 *        using Gauss-Jordan elimination with partial pivoting.
 *
 * @param n     Dimension
 * @param src   Input matrix [n][n]
 * @param dst   Output inverse [n][n]
 * @return 0 on success, -1 if singular
 */
static int mat_invert(uint8_t n, const float src[EKF_MAX_STATES][EKF_MAX_STATES],
                      float dst[EKF_MAX_STATES][EKF_MAX_STATES])
{
    float aug[EKF_MAX_STATES][2 * EKF_MAX_STATES];
    memset(aug, 0, sizeof(aug));

    /* Build augmented matrix [src | I] */
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            aug[i][j] = src[i][j];
        }
        aug[i][n + i] = 1.0f;
    }

    /* Gauss-Jordan elimination */
    for (int col = 0; col < n; col++) {
        /* Partial pivoting */
        int max_row = col;
        float max_val = fabsf(aug[col][col]);
        for (int row = col + 1; row < n; row++) {
            if (fabsf(aug[row][col]) > max_val) {
                max_val = fabsf(aug[row][col]);
                max_row = row;
            }
        }
        if (max_val < 1e-10f) return -1;  /* Singular */

        /* Swap rows */
        if (max_row != col) {
            for (int j = 0; j < 2 * n; j++) {
                float tmp = aug[col][j];
                aug[col][j] = aug[max_row][j];
                aug[max_row][j] = tmp;
            }
        }

        /* Scale pivot row */
        float pivot = aug[col][col];
        for (int j = 0; j < 2 * n; j++) {
            aug[col][j] /= pivot;
        }

        /* Eliminate column */
        for (int row = 0; row < n; row++) {
            if (row == col) continue;
            float factor = aug[row][col];
            for (int j = 0; j < 2 * n; j++) {
                aug[row][j] -= factor * aug[col][j];
            }
        }
    }

    /* Extract inverse */
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            dst[i][j] = aug[i][n + j];
        }
    }

    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int ekf_init(ekf_t *ekf, uint8_t n, const float *Q_diag,
             const float *R_diag, const float *x0, const float *P0_diag)
{
    if (!ekf || n == 0 || n > EKF_MAX_STATES) return -1;

    memset(ekf, 0, sizeof(ekf_t));
    ekf->n = n;

    /* Set initial state */
    for (int i = 0; i < n; i++) {
        ekf->x[i] = x0[i];
    }

    /* Set diagonal covariance matrices */
    for (int i = 0; i < n; i++) {
        ekf->P[i][i] = P0_diag[i];
        ekf->Q[i][i] = Q_diag[i];
        ekf->R[i][i] = R_diag[i];
    }

    ekf->nis = 0.0f;
    ekf->iteration = 0;

    return 0;
}

void ekf_predict(ekf_t *ekf)
{
    if (!ekf) return;
    uint8_t n = ekf->n;

    /*
     * Predict step (identity state transition F = I):
     *   x_pred = F * x = x     (state doesn't change in prediction)
     *   P_pred = F*P*F' + Q = P + Q
     */
    for (int i = 0; i < n; i++) {
        ekf->x_pred[i] = ekf->x[i];
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            ekf->P_pred[i][j] = ekf->P[i][j] + ekf->Q[i][j];
        }
    }
}

void ekf_update(ekf_t *ekf, const float *z)
{
    if (!ekf || !z) return;
    uint8_t n = ekf->n;

    /*
     * Update step (identity measurement matrix H = I):
     *   y = z - H*x_pred = z - x_pred           (innovation)
     *   S = H*P_pred*H' + R = P_pred + R         (innovation covariance)
     *   K = P_pred * S^{-1}                       (Kalman gain)
     *   x = x_pred + K*y                          (updated state)
     *   P = (I - K*H)*P_pred = (I - K)*P_pred     (updated covariance)
     */

    /* Innovation: y = z - x_pred */
    for (int i = 0; i < n; i++) {
        ekf->y[i] = z[i] - ekf->x_pred[i];
    }

    /* Innovation covariance: S = P_pred + R */
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            ekf->S[i][j] = ekf->P_pred[i][j] + ekf->R[i][j];
        }
    }

    /* Invert S */
    float S_inv[EKF_MAX_STATES][EKF_MAX_STATES];
    memset(S_inv, 0, sizeof(S_inv));
    if (mat_invert(n, ekf->S, S_inv) != 0) {
        /* S is singular — skip update, keep prediction */
        for (int i = 0; i < n; i++) {
            ekf->x[i] = ekf->x_pred[i];
        }
        memcpy(ekf->P, ekf->P_pred, sizeof(ekf->P));
        ekf->nis = 999.0f;
        ekf->iteration++;
        return;
    }

    /* Kalman gain: K = P_pred * S_inv */
    memset(ekf->K, 0, sizeof(ekf->K));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            for (int k = 0; k < n; k++) {
                ekf->K[i][j] += ekf->P_pred[i][k] * S_inv[k][j];
            }
        }
    }

    /* Updated state: x = x_pred + K * y */
    for (int i = 0; i < n; i++) {
        ekf->x[i] = ekf->x_pred[i];
        for (int j = 0; j < n; j++) {
            ekf->x[i] += ekf->K[i][j] * ekf->y[j];
        }
    }

    /* Updated covariance: P = (I - K) * P_pred */
    float IminusK[EKF_MAX_STATES][EKF_MAX_STATES];
    memset(IminusK, 0, sizeof(IminusK));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            IminusK[i][j] = ((i == j) ? 1.0f : 0.0f) - ekf->K[i][j];
        }
    }

    float P_new[EKF_MAX_STATES][EKF_MAX_STATES];
    memset(P_new, 0, sizeof(P_new));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            for (int k = 0; k < n; k++) {
                P_new[i][j] += IminusK[i][k] * ekf->P_pred[k][j];
            }
        }
    }
    memcpy(ekf->P, P_new, sizeof(ekf->P));

    /* Compute NIS: NIS = y' * S_inv * y */
    ekf->nis = 0.0f;
    for (int i = 0; i < n; i++) {
        float tmp = 0.0f;
        for (int j = 0; j < n; j++) {
            tmp += S_inv[i][j] * ekf->y[j];
        }
        ekf->nis += ekf->y[i] * tmp;
    }

    ekf->iteration++;
}

void ekf_get_state(const ekf_t *ekf, float *x)
{
    if (!ekf || !x) return;
    for (int i = 0; i < ekf->n; i++) {
        x[i] = ekf->x[i];
    }
}

float ekf_get_nis(const ekf_t *ekf)
{
    if (!ekf) return 0.0f;
    return ekf->nis;
}

void ekf_get_innovation(const ekf_t *ekf, float *y)
{
    if (!ekf || !y) return;
    for (int i = 0; i < ekf->n; i++) {
        y[i] = ekf->y[i];
    }
}

void ekf_reset(ekf_t *ekf, const float *x0, const float *P0_diag)
{
    if (!ekf || !x0 || !P0_diag) return;

    for (int i = 0; i < ekf->n; i++) {
        ekf->x[i] = x0[i];
    }

    memset(ekf->P, 0, sizeof(ekf->P));
    for (int i = 0; i < ekf->n; i++) {
        ekf->P[i][i] = P0_diag[i];
    }

    ekf->nis = 0.0f;
    ekf->iteration = 0;
}

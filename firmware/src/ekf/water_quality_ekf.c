/**
 * @file water_quality_ekf.c
 * @brief Water quality EKF implementation with domain-specific tuning
 */

#include "ekf/water_quality_ekf.h"
#include "calibration.h"
#include <string.h>

/* EKF tuning parameters (from plan section 5.2) */
static const float wq_Q_diag[WQ_NUM_STATES] = {
    0.01f,  /* pH — changes slowly */
    0.50f,  /* Turbidity — can change faster (rain, runoff) */
    0.05f,  /* DO — moderate change rate */
    0.01f   /* Temperature — changes slowly */
};

static const float wq_R_diag[WQ_NUM_STATES] = {
    0.05f,  /* pH sensor noise (from datasheet) */
    2.00f,  /* Turbidity sensor noise */
    0.10f,  /* DO sensor noise */
    0.02f   /* DS18B20 noise (very low) */
};

static const float wq_P0_diag[WQ_NUM_STATES] = {
    1.0f,   /* Initial pH uncertainty */
    10.0f,  /* Initial turbidity uncertainty */
    1.0f,   /* Initial DO uncertainty */
    1.0f    /* Initial temperature uncertainty */
};

static const float wq_x0_default[WQ_NUM_STATES] = {
    7.0f,   /* Neutral pH */
    0.0f,   /* Clear water */
    8.0f,   /* Typical DO at 25°C */
    25.0f   /* Room temperature */
};

/* Maximum consecutive anomalies before EKF reset */
#define WQ_EKF_MAX_CONSECUTIVE_ANOMALIES  10

int wq_ekf_init(wq_ekf_t *wq)
{
    if (!wq) return -1;

    wq->consecutive_anomalies = 0;

    return ekf_init(&wq->ekf, WQ_NUM_STATES,
                    wq_Q_diag, wq_R_diag, wq_x0_default, wq_P0_diag);
}

int wq_ekf_init_from_measurement(wq_ekf_t *wq, const sensor_data_t *data)
{
    if (!wq || !data) return -1;

    float x0[WQ_NUM_STATES] = {
        data->ph,
        data->turbidity,
        data->do_mg_l,
        data->temp_c
    };

    wq->consecutive_anomalies = 0;

    return ekf_init(&wq->ekf, WQ_NUM_STATES,
                    wq_Q_diag, wq_R_diag, x0, wq_P0_diag);
}

void wq_ekf_update(wq_ekf_t *wq, const sensor_data_t *data)
{
    if (!wq || !data) return;

    /* Predict */
    ekf_predict(&wq->ekf);

    /* Form measurement vector */
    float z[WQ_NUM_STATES] = {
        data->ph,
        data->turbidity,
        data->do_mg_l,
        data->temp_c
    };

    /* Update */
    ekf_update(&wq->ekf, z);

    /* Track consecutive anomalies for auto-reset */
    if (ekf_get_nis(&wq->ekf) > ANOMALY_NIS_THRESHOLD) {
        wq->consecutive_anomalies++;
    } else {
        wq->consecutive_anomalies = 0;
    }

    /* Auto-reset if diverged */
    if (wq->consecutive_anomalies >= WQ_EKF_MAX_CONSECUTIVE_ANOMALIES) {
        ekf_reset(&wq->ekf, z, wq_P0_diag);
        wq->consecutive_anomalies = 0;
    }
}

void wq_ekf_get_filtered(const wq_ekf_t *wq, sensor_data_t *data)
{
    if (!wq || !data) return;

    float x[WQ_NUM_STATES];
    ekf_get_state(&wq->ekf, x);

    data->ph        = x[WQ_STATE_PH];
    data->turbidity = x[WQ_STATE_TURB];
    data->do_mg_l   = x[WQ_STATE_DO];
    data->temp_c    = x[WQ_STATE_TEMP];
    data->valid     = true;
}

int wq_ekf_needs_reset(const wq_ekf_t *wq)
{
    if (!wq) return 0;
    return wq->consecutive_anomalies >= WQ_EKF_MAX_CONSECUTIVE_ANOMALIES;
}

/**
 * @file anomaly_detector.c
 * @brief Anomaly detection implementation
 *
 * Checks both individual parameter thresholds and the EKF's
 * Normalised Innovation Squared (NIS) against a chi-squared
 * threshold for statistical anomaly detection.
 */

#include "ekf/anomaly_detector.h"
#include "calibration.h"

anomaly_result_t anomaly_detector_check(const wq_ekf_t *wq,
                                         const sensor_data_t *data)
{
    anomaly_result_t result = {0};

    if (!wq || !data) {
        return result;
    }

    /* Threshold-based checks */
    if (data->ph < ANOMALY_PH_LOW || data->ph > ANOMALY_PH_HIGH) {
        result.flags |= ANOMALY_FLAG_PH;
        result.ph = data->ph;
    }

    if (data->turbidity > ANOMALY_TURB_HIGH) {
        result.flags |= ANOMALY_FLAG_TURB;
        result.turbidity = data->turbidity;
    }

    if (data->do_mg_l < ANOMALY_DO_LOW) {
        result.flags |= ANOMALY_FLAG_DO;
        result.do_mg_l = data->do_mg_l;
    }

    if (data->temp_c > ANOMALY_TEMP_HIGH) {
        result.flags |= ANOMALY_FLAG_TEMP;
        result.temp_c = data->temp_c;
    }

    /* EKF statistical anomaly (NIS chi-squared test) */
    result.nis = ekf_get_nis(&wq->ekf);
    if (result.nis > ANOMALY_NIS_THRESHOLD) {
        result.flags |= ANOMALY_FLAG_NIS;
    }

    return result;
}

int anomaly_is_flagged(const anomaly_result_t *result)
{
    if (!result) return 0;
    return (result->flags != 0) ? 1 : 0;
}

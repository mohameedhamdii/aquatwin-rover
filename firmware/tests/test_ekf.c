/**
 * @file test_ekf.c
 * @brief Unit tests for Extended Kalman Filter & Anomaly Detector
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "ekf/ekf.h"
#include "ekf/water_quality_ekf.h"
#include "ekf/anomaly_detector.h"
#include "sensors/sensor_manager.h"

#define ASSERT_NEAR(val, target, eps) \
    do { \
        float diff = fabsf((float)(val) - (float)(target)); \
        if (diff > (eps)) { \
            fprintf(stderr, "FAIL: %s:%d: Expected %f ~= %f (diff=%f > %f)\n", \
                    __FILE__, __LINE__, (double)(val), (double)(target), (double)diff, (double)(eps)); \
            exit(1); \
        } \
    } while(0)

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s:%d: Condition failed: %s\n", __FILE__, __LINE__, #cond); \
            exit(1); \
        } \
    } while(0)

static void test_ekf_initialization(void)
{
    printf("[TEST] EKF initialization...\n");
    wq_ekf_t wq;
    int rc = wq_ekf_init(&wq);
    ASSERT_TRUE(rc == 0);
    ASSERT_TRUE(wq.consecutive_anomalies == 0);

    /* Verify initial states match default nominal values */
    sensor_data_t out;
    wq_ekf_get_filtered(&wq, &out);
    ASSERT_NEAR(out.ph, 7.0f, 0.01f);
    ASSERT_NEAR(out.turbidity, 5.0f, 0.01f);
    ASSERT_NEAR(out.do_mg_l, 8.0f, 0.01f);
    ASSERT_NEAR(out.temp_c, 20.0f, 0.01f);
    printf("  PASS: EKF initializes with correct nominal states\n");
}

static void test_ekf_convergence(void)
{
    printf("[TEST] EKF convergence under steady-state measurements...\n");
    wq_ekf_t wq;
    wq_ekf_init(&wq);

    /* Feed steady stream of clean measurements: pH=7.4, Turb=8.0, DO=7.5, Temp=22.0 */
    sensor_data_t input = {
        .ph = 7.4f,
        .turbidity = 8.0f,
        .do_mg_l = 7.5f,
        .temp_c = 22.0f,
        .valid = true,
        .timestamp_ms = 1000
    };

    for (int step = 0; step < 25; step++) {
        wq_ekf_update(&wq, &input);
    }

    sensor_data_t filtered;
    wq_ekf_get_filtered(&wq, &filtered);

    ASSERT_NEAR(filtered.ph, 7.4f, 0.05f);
    ASSERT_NEAR(filtered.turbidity, 8.0f, 0.5f);
    ASSERT_NEAR(filtered.do_mg_l, 7.5f, 0.1f);
    ASSERT_NEAR(filtered.temp_c, 22.0f, 0.05f);
    printf("  PASS: Filtered states converged accurately to true inputs\n");
}

static void test_anomaly_detection_thresholds(void)
{
    printf("[TEST] Anomaly detection threshold alerts...\n");
    wq_ekf_t wq;
    wq_ekf_init(&wq);

    /* 1. Acidic pH alert (pH < 6.0) */
    sensor_data_t acid_input = { .ph = 5.2f, .turbidity = 10.0f, .do_mg_l = 7.0f, .temp_c = 21.0f, .valid = true };
    anomaly_result_t res1 = anomaly_detector_check(&wq, &acid_input);
    ASSERT_TRUE(anomaly_is_flagged(&res1));
    ASSERT_TRUE(res1.flags & ANOMALY_FLAG_PH);

    /* 2. Turbidity spike alert (Turbidity > 50 NTU) */
    sensor_data_t turb_input = { .ph = 7.2f, .turbidity = 65.0f, .do_mg_l = 7.0f, .temp_c = 21.0f, .valid = true };
    anomaly_result_t res2 = anomaly_detector_check(&wq, &turb_input);
    ASSERT_TRUE(anomaly_is_flagged(&res2));
    ASSERT_TRUE(res2.flags & ANOMALY_FLAG_TURB);

    /* 3. Hypoxia alert (DO < 4.0 mg/L) */
    sensor_data_t do_input = { .ph = 7.2f, .turbidity = 10.0f, .do_mg_l = 2.8f, .temp_c = 21.0f, .valid = true };
    anomaly_result_t res3 = anomaly_detector_check(&wq, &do_input);
    ASSERT_TRUE(anomaly_is_flagged(&res3));
    ASSERT_TRUE(res3.flags & ANOMALY_FLAG_DO);

    /* 4. Thermal pollution alert (Temp > 35 °C) */
    sensor_data_t temp_input = { .ph = 7.2f, .turbidity = 10.0f, .do_mg_l = 7.0f, .temp_c = 38.5f, .valid = true };
    anomaly_result_t res4 = anomaly_detector_check(&wq, &temp_input);
    ASSERT_TRUE(anomaly_is_flagged(&res4));
    ASSERT_TRUE(res4.flags & ANOMALY_FLAG_TEMP);

    /* 5. Safe nominal conditions */
    sensor_data_t normal_input = { .ph = 7.3f, .turbidity = 8.0f, .do_mg_l = 7.5f, .temp_c = 21.5f, .valid = true };
    anomaly_result_t res5 = anomaly_detector_check(&wq, &normal_input);
    ASSERT_TRUE(!anomaly_is_flagged(&res5));
    printf("  PASS: Boundary and safety thresholds correctly trigger flags\n");
}

static void test_ekf_nis_and_reset(void)
{
    printf("[TEST] EKF statistical innovation (NIS) & reset counter...\n");
    wq_ekf_t wq;
    wq_ekf_init(&wq);

    /* Stabilize at baseline */
    sensor_data_t baseline = { .ph = 7.0f, .turbidity = 5.0f, .do_mg_l = 8.0f, .temp_c = 20.0f, .valid = true };
    for (int i = 0; i < 15; i++) {
        wq_ekf_update(&wq, &baseline);
    }

    /* Introduce wild anomaly causing high NIS */
    sensor_data_t shock = { .ph = 3.0f, .turbidity = 95.0f, .do_mg_l = 0.5f, .temp_c = 42.0f, .valid = true };
    anomaly_result_t shock_res = anomaly_detector_check(&wq, &shock);
    ASSERT_TRUE(shock_res.nis > 13.28f); /* Chi-squared 4-DOF 99% threshold is 13.28 */
    ASSERT_TRUE(shock_res.flags & ANOMALY_FLAG_NIS);

    /* Confirm consecutive anomaly counter increments */
    for (int i = 0; i < 12; i++) {
        wq_ekf_update(&wq, &shock);
    }
    ASSERT_TRUE(wq_ekf_needs_reset(&wq));
    printf("  PASS: High statistical NIS detected and reset triggered after prolonged divergence\n");
}

int main(void)
{
    printf("=== RUNNING EKF UNIT TESTS ===\n");
    test_ekf_initialization();
    test_ekf_convergence();
    test_anomaly_detection_thresholds();
    test_ekf_nis_and_reset();
    printf("=== ALL EKF TESTS PASSED SUCCESSFULLY ===\n\n");
    return 0;
}

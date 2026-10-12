/**
 * @file test_sensors.c
 * @brief Unit tests for pH, Turbidity, and Dissolved Oxygen sensor calculations
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "sensors/ph_sensor.h"
#include "sensors/turbidity_sensor.h"
#include "sensors/do_sensor.h"
#include "calibration.h"

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

static void test_ph_sensor_calculation(void)
{
    printf("[TEST] pH sensor calibration & temperature compensation...\n");
    ph_sensor_t ph;
    int rc = ph_sensor_init(&ph);
    ASSERT_TRUE(rc == 0);

    /* Test neutral buffer (pH 7.0 voltage) */
    /* Voltage corresponding to neutral: PH_CAL_V_NEUTRAL_MV = 1500 mV */
    /* 12-bit ADC mapping: adc = (1500 / 3300) * 4095 ~= 1861 */
    uint16_t adc_neutral = (uint16_t)((PH_CAL_V_NEUTRAL_MV / 3300.0f) * 4095.0f);
    for (int i = 0; i < 10; i++) {
        ph_sensor_accumulate(&ph, adc_neutral);
    }
    float computed_ph = ph_sensor_compute(&ph, 25.0f);
    ASSERT_NEAR(computed_ph, 7.0f, 0.1f);

    /* Test acidic buffer (pH 4.0 voltage) */
    uint16_t adc_acid = (uint16_t)((PH_CAL_V_ACID_MV / 3300.0f) * 4095.0f);
    for (int i = 0; i < 10; i++) {
        ph_sensor_accumulate(&ph, adc_acid);
    }
    float computed_acid_ph = ph_sensor_compute(&ph, 25.0f);
    ASSERT_NEAR(computed_acid_ph, 4.0f, 0.1f);

    /* Test temperature effect: at higher temperature, slope scales proportionally */
    for (int i = 0; i < 10; i++) {
        ph_sensor_accumulate(&ph, adc_acid);
    }
    float ph_hot = ph_sensor_compute(&ph, 45.0f);
    ASSERT_TRUE(ph_hot > 0.0f && ph_hot < 14.0f);

    printf("  PASS: pH 7.0 and pH 4.0 calibrations accurately computed with temperature compensation\n");
}

static void test_turbidity_sensor_calculation(void)
{
    printf("[TEST] Turbidity sensor voltage-to-NTU calculation...\n");
    turb_sensor_t turb;
    int rc = turb_sensor_init(&turb);
    ASSERT_TRUE(rc == 0);

    /* Clean water (high voltage, near 4.1V or 3.3V ADC full scale) */
    uint16_t adc_clear = 4000;
    for (int i = 0; i < 10; i++) {
        turb_sensor_accumulate(&turb, adc_clear);
    }
    float clear_ntu = turb_sensor_compute(&turb);
    ASSERT_TRUE(clear_ntu <= 10.0f); /* Clear water should be low NTU */

    /* Murky water (lower voltage, e.g. 2.0V) */
    /* 2.0V / 3.3V * 4095 ~= 2480 */
    uint16_t adc_murky = 2480;
    for (int i = 0; i < 10; i++) {
        turb_sensor_accumulate(&turb, adc_murky);
    }
    float murky_ntu = turb_sensor_compute(&turb);
    ASSERT_TRUE(murky_ntu > clear_ntu);

    printf("  PASS: Turbidity polynomial mapping correctly reflects optical clarity\n");
}

static void test_do_sensor_calculation(void)
{
    printf("[TEST] Dissolved oxygen saturation and reading...\n");
    do_sensor_t dox;
    int rc = do_sensor_init(&dox);
    ASSERT_TRUE(rc == 0);

    /* Test saturation lookup table */
    float sat_0 = do_saturation_at_temp(0.0f);
    float sat_20 = do_saturation_at_temp(20.0f);
    float sat_30 = do_saturation_at_temp(30.0f);

    /* Cold water dissolves more oxygen than warm water */
    ASSERT_TRUE(sat_0 > sat_20);
    ASSERT_TRUE(sat_20 > sat_30);
    ASSERT_NEAR(sat_20, 9.09f, 0.5f); /* Standard saturation at 20°C is ~9.09 mg/L */

    /* Reference voltage accumulation should yield reference saturation */
    uint16_t adc_ref = (uint16_t)((DO_CAL_V_REF_MV / 3300.0f) * 4095.0f);
    for (int i = 0; i < 10; i++) {
        do_sensor_accumulate(&dox, adc_ref);
    }
    float computed_do = do_sensor_compute(&dox, DO_CAL_TEMP_C);
    ASSERT_NEAR(computed_do, DO_CAL_SAT_MG_L, 0.5f);

    printf("  PASS: Dissolved Oxygen saturation curve and calibrated readings verified\n");
}

int main(void)
{
    printf("=== RUNNING SENSOR DRIVER UNIT TESTS ===\n");
    test_ph_sensor_calculation();
    test_turbidity_sensor_calculation();
    test_do_sensor_calculation();
    printf("=== ALL SENSOR TESTS PASSED SUCCESSFULLY ===\n\n");
    return 0;
}

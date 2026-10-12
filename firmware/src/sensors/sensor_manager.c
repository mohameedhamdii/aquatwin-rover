/**
 * @file sensor_manager.c
 * @brief Sensor manager implementation — coordinates all sensor reads
 *
 * Performs 10x oversampling on ADC sensors (100 ms at 10 Hz),
 * reads DS18B20 temperature, then computes calibrated values.
 */

#include "sensors/sensor_manager.h"
#include "pin_map.h"
#include "calibration.h"
#include "stm32l4xx_hal.h"

/* ============================================================================
 * ADC channel reading helper
 * ============================================================================ */

uint16_t sensor_manager_read_adc(ADC_HandleTypeDef *hadc, uint32_t channel)
{
    ADC_ChannelConfTypeDef config = {0};
    config.Channel      = channel;
    config.Rank         = ADC_REGULAR_RANK_1;
    config.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
    config.SingleDiff   = ADC_SINGLE_ENDED;
    config.OffsetNumber = ADC_OFFSET_NONE;
    config.Offset       = 0;

    HAL_ADC_ConfigChannel(hadc, &config);
    HAL_ADC_Start(hadc);
    HAL_ADC_PollForConversion(hadc, 10);
    uint16_t value = (uint16_t)HAL_ADC_GetValue(hadc);
    HAL_ADC_Stop(hadc);

    return value;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int sensor_manager_init(sensor_manager_t *mgr, ADC_HandleTypeDef *hadc)
{
    if (!mgr || !hadc) return -1;

    mgr->hadc = hadc;

    /* Calibrate ADC */
    HAL_ADCEx_Calibration_Start(hadc, ADC_SINGLE_ENDED);

    /* Initialise each sensor driver */
    int rc;

    rc = ph_sensor_init(&mgr->ph);
    if (rc != 0) return rc;

    rc = turb_sensor_init(&mgr->turb);
    if (rc != 0) return rc;

    rc = do_sensor_init(&mgr->dox);
    if (rc != 0) return rc;

    rc = temp_sensor_init(&mgr->temp);
    if (rc != 0) {
        /* Temperature sensor not found — continue with a default value.
         * This allows testing without the DS18B20 connected. */
    }

    mgr->initialised = true;
    return 0;
}

sensor_data_t sensor_manager_read_all(sensor_manager_t *mgr)
{
    sensor_data_t result = {0};
    result.valid = false;

    if (!mgr || !mgr->initialised) return result;

    /* Step 1: Start DS18B20 temperature conversion (takes ~750 ms in background) */
    temp_sensor_start_conversion(&mgr->temp);

    /* Step 2: Oversample ADC sensors — 10 samples at 10 Hz = 1 second */
    for (int i = 0; i < ADC_OVERSAMPLE_COUNT; i++) {
        uint16_t ph_raw   = sensor_manager_read_adc(mgr->hadc, PH_SENSOR_ADC_CHANNEL);
        uint16_t turb_raw = sensor_manager_read_adc(mgr->hadc, TURB_SENSOR_ADC_CHANNEL);
        uint16_t do_raw   = sensor_manager_read_adc(mgr->hadc, DO_SENSOR_ADC_CHANNEL);

        ph_sensor_accumulate(&mgr->ph, ph_raw);
        turb_sensor_accumulate(&mgr->turb, turb_raw);
        do_sensor_accumulate(&mgr->dox, do_raw);

        HAL_Delay(100);  /* 10 Hz sampling */
    }

    /* Step 3: Read temperature result */
    float temp_c = temp_sensor_read(&mgr->temp);
    if (temp_c < -100.0f) {
        /* DS18B20 read failed — use last known value */
        temp_c = temp_sensor_get_last(&mgr->temp);
        if (temp_c < -100.0f) {
            temp_c = 25.0f;  /* Fallback default */
        }
    }

    /* Step 4: Compute calibrated values */
    result.ph        = ph_sensor_compute(&mgr->ph, temp_c);
    result.turbidity = turb_sensor_compute(&mgr->turb);
    result.do_mg_l   = do_sensor_compute(&mgr->dox, temp_c);
    result.temp_c    = temp_c;
    result.timestamp_ms = HAL_GetTick();

    /* Validate */
    result.valid = (result.ph >= PH_VALID_MIN && result.ph <= PH_VALID_MAX)
                && (result.turbidity >= TURB_VALID_MIN)
                && (result.do_mg_l >= DO_VALID_MIN)
                && (result.temp_c > TEMP_VALID_MIN && result.temp_c < TEMP_VALID_MAX);

    return result;
}

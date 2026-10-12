/**
 * @file calibration.h
 * @brief Sensor calibration constants for the Water Quality Monitor
 *
 * These values MUST be updated after performing the calibration procedure
 * described in docs/sensor_calibration_guide.md.
 * 
 * Default values are approximate and should not be used for production
 * measurements without proper calibration.
 */

#ifndef CALIBRATION_H
#define CALIBRATION_H

/* ============================================================================
 * ADC Configuration
 * ============================================================================ */
#define ADC_RESOLUTION_BITS         12
#define ADC_MAX_VALUE               4095            /* 2^12 - 1 */
#define ADC_VREF_MV                 3300            /* 3.3V reference in millivolts */

/* Convert raw ADC to millivolts */
#define ADC_TO_MV(raw)  ((uint32_t)(raw) * ADC_VREF_MV / ADC_MAX_VALUE)

/* Number of ADC samples to average per reading (10 Hz sampling, 1 Hz output) */
#define ADC_OVERSAMPLE_COUNT        10

/* ============================================================================
 * pH Sensor Calibration — DFRobot SEN0161-V2
 * ============================================================================
 * Two-point calibration using pH 4.0 and pH 7.0 buffer solutions.
 * Procedure:
 *   1. Immerse probe in pH 7.0 buffer, record stable ADC voltage → PH_CAL_V_NEUTRAL
 *   2. Immerse probe in pH 4.0 buffer, record stable ADC voltage → PH_CAL_V_ACID
 *   3. Compute slope: (7.0 - 4.0) / (V_neutral - V_acid)
 *   4. Runtime: pH = 7.0 + (V_measured - V_neutral) * slope
 */
#define PH_CAL_V_NEUTRAL_MV        1500            /* Voltage at pH 7.0 (mV) — CALIBRATE! */
#define PH_CAL_V_ACID_MV           2032            /* Voltage at pH 4.0 (mV) — CALIBRATE! */
#define PH_CAL_NEUTRAL_PH          7.0f
#define PH_CAL_ACID_PH             4.0f

/* Nernst equation temperature compensation coefficient (mV/°C/pH unit) */
#define PH_NERNST_COEFF            0.1984f         /* ≈ R·ln(10)/F in mV/°C */
#define PH_CAL_REFERENCE_TEMP_C    25.0f           /* Temperature at which calibration was done */

/* Valid pH range (readings outside this are sensor errors) */
#define PH_VALID_MIN                0.0f
#define PH_VALID_MAX                14.0f

/* ============================================================================
 * Turbidity Sensor Calibration — DFRobot SEN0189
 * ============================================================================
 * Linear mapping from voltage to NTU.
 * Procedure:
 *   1. Measure voltage in clean water → TURB_CAL_V_CLEAR_MV
 *   2. Measure voltage in known turbid solution → use datasheet curve
 *   3. Apply polynomial or linear fit
 */
#define TURB_CAL_V_CLEAR_MV        4200            /* Voltage in clear water (mV) — CALIBRATE! */
#define TURB_CAL_V_MAX_NTU_MV      850             /* Voltage at max turbidity (mV) */
#define TURB_CAL_MAX_NTU           3000.0f         /* Maximum NTU range */

/* Polynomial coefficients: NTU = a2*V^2 + a1*V + a0 (V in volts) */
#define TURB_POLY_A2               (-1120.4f)
#define TURB_POLY_A1               (5742.3f)
#define TURB_POLY_A0               (-4352.9f)

/* Valid turbidity range */
#define TURB_VALID_MIN              0.0f
#define TURB_VALID_MAX              4000.0f

/* ============================================================================
 * Dissolved Oxygen Sensor Calibration — DFRobot SEN0237-A
 * ============================================================================
 * Single-point calibration in air-saturated water.
 * Procedure:
 *   1. Place probe in air-saturated water at known temperature
 *   2. Record stable ADC voltage → DO_CAL_V_REFERENCE_MV
 *   3. Look up saturation DO at calibration temperature → DO_CAL_SATURATION_MG_L
 *   4. Runtime: DO = (V_measured / V_reference) * saturation_DO(current_temp)
 */
#define DO_CAL_V_REFERENCE_MV      1460            /* Voltage in air-saturated water (mV) — CALIBRATE! */
#define DO_CAL_REFERENCE_TEMP_C    25.0f           /* Temperature during calibration */
#define DO_CAL_SATURATION_MG_L     8.26f           /* Saturated DO at 25°C, sea level */

/* DO saturation lookup table (mg/L at 1 atm, per °C, 0-40°C) */
#define DO_SAT_TABLE_SIZE           41
/* Table values from Standard Methods for Examination of Water & Wastewater */

/* Valid DO range */
#define DO_VALID_MIN                0.0f
#define DO_VALID_MAX                20.0f

/* ============================================================================
 * Temperature Sensor — DS18B20
 * ============================================================================
 * Factory-calibrated, no user calibration needed.
 */
#define TEMP_VALID_MIN              (-10.0f)
#define TEMP_VALID_MAX              (85.0f)         /* DS18B20 max range */

/* ============================================================================
 * Battery Monitoring
 * ============================================================================
 * Voltage divider: R1 = 100kΩ (top), R2 = 100kΩ (bottom)
 * V_adc = V_bat * R2 / (R1 + R2) = V_bat * 0.5
 */
#define BATTERY_DIVIDER_RATIO       2.0f            /* Multiply ADC voltage by this */
#define BATTERY_FULL_MV             4200            /* Fully charged LiPo */
#define BATTERY_EMPTY_MV            3200            /* Cutoff voltage */
#define BATTERY_LOW_THRESHOLD_PCT   20              /* Reduce TX rate below this */
#define BATTERY_CRITICAL_PCT        10              /* Deep sleep below this */

/* ============================================================================
 * Solar Panel Monitoring
 * ============================================================================
 * Voltage divider: R1 = 100kΩ, R2 = 47kΩ
 * V_adc = V_panel * R2 / (R1 + R2) = V_panel * 0.32
 */
#define SOLAR_DIVIDER_RATIO         3.128f          /* (100+47)/47 */
#define SOLAR_PANEL_MAX_MV          6000            /* 6V panel nominal */
#define SOLAR_CHARGING_MIN_MV       4000            /* Min panel voltage to charge */

/* ============================================================================
 * Anomaly Detection Thresholds
 * ============================================================================ */
#define ANOMALY_PH_LOW              6.0f
#define ANOMALY_PH_HIGH             9.0f
#define ANOMALY_TURB_HIGH           50.0f           /* NTU */
#define ANOMALY_DO_LOW              4.0f            /* mg/L */
#define ANOMALY_TEMP_HIGH           35.0f           /* °C */

/* Chi-squared threshold for NIS with 4 DOF at 99% confidence */
#define ANOMALY_NIS_THRESHOLD       13.28f

#endif /* CALIBRATION_H */

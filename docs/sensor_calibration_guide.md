# Water Quality Sensor Calibration Guide

This guide details the exact calibration workflows for the 4 physical water quality sensors on the AquaTwin rover/buoy.
Accurate calibration is vital for the on-device Extended Kalman Filter (EKF) to reliably differentiate real contamination anomalies from sensor offset drift.

---

## 1. pH Sensor (DFRobot Gravity Analog pH Sensor V2 / SEN0161-V2)

The probe measures hydrogen ion activity via an electrochemical glass electrode. It requires **two-point calibration** using standard buffer solutions.

### Required Materials
- Standard buffer solution pH 7.00 (neutral)
- Standard buffer solution pH 4.00 (acidic)
- Distilled water (for rinsing probe between tests)
- Beaker and lint-free wipes

### Step-by-Step Procedure
1. **Connect & Power:** Connect the probe BNC connector to the signal conditioning board. Ensure the MCU ADC is powered with a stable 3.3V reference.
2. **Neutral Buffer Measurement (pH 7.00):**
   - Rinse probe thoroughly with distilled water and dry gently.
   - Submerge probe tip into the pH 7.00 buffer. Stir gently and wait 60 seconds for equilibrium.
   - Read the raw ADC millivolt output over UART debug console:
     ```bash
     [CALIB] pH Raw ADC: 1861 mV @ 24.5 °C
     ```
   - Record this value as `PH_CAL_V_NEUTRAL_MV`.
3. **Acidic Buffer Measurement (pH 4.00):**
   - Rinse probe with distilled water.
   - Submerge probe tip into the pH 4.00 buffer. Wait 60 seconds.
   - Read the raw ADC millivolts:
     ```bash
     [CALIB] pH Raw ADC: 1245 mV @ 24.5 °C
     ```
   - Record this value as `PH_CAL_V_ACID_MV`.
4. **Update Firmware Constants:**
   Open `firmware/config/calibration.h` and update:
   ```c
   #define PH_CAL_V_NEUTRAL_MV     1861.0f  /* Voltage at pH 7.0 */
   #define PH_CAL_V_ACID_MV        1245.0f  /* Voltage at pH 4.0 */
   #define PH_CAL_TEMP_C           24.5f    /* Calibration temperature */
   ```
5. **Runtime Nernst Compensation:**
   The firmware automatically applies the Nernst equation:
   $$\text{Slope}_T = \text{Slope}_{\text{cal}} \times \frac{273.15 + T_{\text{water}}}{273.15 + T_{\text{cal}}}$$
   Where $T_{\text{water}}$ is continuously sampled from the waterproof DS18B20 probe.

---

## 2. Turbidity Sensor (DFRobot SEN0189)

The turbidity sensor uses an infrared emitter and phototransistor to measure optical light scattering caused by suspended particulates.

### Calibration Procedure
1. **Clear Water Baseline (0 NTU):**
   - Submerge optical sensor head into a jar of clean distilled water.
   - Note the output voltage: should typically measure between $4.10\text{ V}$ and $4.25\text{ V}$ (on a 5V supply) or around $3.10\text{ V}$ to $3.25\text{ V}$ on a 3.3V voltage divider.
   - Record as `TURB_CAL_V_CLEAR`.
2. **Turbid Standard (1000 NTU):**
   - Submerge probe into a calibrated 1000 NTU Formazin standard solution.
   - Record the voltage as `TURB_CAL_V_TURBID`.
3. **Polynomial Fit:**
   The sensor transfer function is quadratic:
   $$\text{NTU} = a_2 \cdot V^2 + a_1 \cdot V + a_0$$
   Configure these coefficients in `firmware/config/calibration.h`.

---

## 3. Dissolved Oxygen (DO) Sensor (SEN0237-A)

The galvanic DO probe produces an electrical potential directly proportional to the partial pressure of dissolved oxygen.

### Calibration Procedure (Air-Saturated Water Method)
1. **Prepare Air-Saturated Water:**
   - Fill a 500 mL beaker with 300 mL of clean distilled or tap water.
   - Use an aquarium air pump with an air stone to bubble air through the water vigorously for 15 minutes.
   - This saturates the water with 100% dissolved oxygen at the current ambient barometric pressure and temperature.
2. **Polarize the Probe:**
   - Connect the probe and leave powered for 5–10 minutes to allow the galvanic reaction to stabilize.
3. **Record Voltage & Temperature:**
   - Submerge the probe in the air-saturated water.
   - Record the stable ADC voltage in millivolts and the temperature from the DS18B20.
   - Lookup the 100% saturation DO concentration at this temperature (e.g. at 20°C, saturation is $9.09\text{ mg/L}$).
4. **Update `calibration.h`:**
   ```c
   #define DO_CAL_V_REF_MV         1600.0f  /* Voltage in saturated water */
   #define DO_CAL_TEMP_C           20.0f    /* Temperature during calibration */
   #define DO_CAL_SAT_MG_L         9.09f    /* Saturated DO at 20°C */
   ```

---

## 4. Temperature Probe (DS18B20)

The Maxim DS18B20 is factory-calibrated and communicates over the digital 1-Wire protocol.
- Accuracy: $\pm 0.5\text{ °C}$ from $-10\text{ °C}$ to $+85\text{ °C}$.
- Resolution: 12-bit ($0.0625\text{ °C}$ step).
- To verify, submerge into an ice-water bath ($0.0\text{ °C} \pm 0.3\text{ °C}$) and a boiling water bath at known atmospheric pressure. No calibration constants are required.

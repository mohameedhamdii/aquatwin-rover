/**
 * @file power_manager.c
 * @brief Power management implementation
 *
 * Manages adaptive power modes and STM32L4 STOP mode sleep.
 * - Normal: 60s TX interval
 * - Low power: 300s TX interval (battery < 20%)
 * - Critical: deep sleep until solar charging resumes (battery < 10%)
 */

#include "power/power_manager.h"
#include "calibration.h"
#include "lora_config.h"
#include "stm32l4xx_hal.h"

int power_manager_init(power_manager_t *pm, ADC_HandleTypeDef *hadc)
{
    if (!pm || !hadc) return -1;

    battery_monitor_init(&pm->battery, hadc);
    solar_monitor_init(&pm->solar, hadc);

    pm->mode             = POWER_MODE_NORMAL;
    pm->sleep_duration_s = LORA_TX_INTERVAL_S;
    pm->initialised      = true;

    return 0;
}

void power_manager_update(power_manager_t *pm)
{
    if (!pm || !pm->initialised) return;

    /* Read current power state */
    battery_monitor_read_mv(&pm->battery);
    solar_monitor_read_mv(&pm->solar);

    /* Determine power mode */
    if (battery_monitor_is_critical(&pm->battery) &&
        !solar_monitor_is_charging(&pm->solar)) {
        pm->mode = POWER_MODE_CRITICAL;
        pm->sleep_duration_s = 0;  /* Will enter deep sleep */
    } else if (battery_monitor_is_low(&pm->battery)) {
        pm->mode = POWER_MODE_LOW_POWER;
        pm->sleep_duration_s = LORA_TX_INTERVAL_LOW_BAT_S;
    } else {
        pm->mode = POWER_MODE_NORMAL;
        pm->sleep_duration_s = LORA_TX_INTERVAL_S;
    }
}

uint32_t power_manager_get_sleep_duration(const power_manager_t *pm)
{
    if (!pm) return LORA_TX_INTERVAL_S;
    return pm->sleep_duration_s;
}

power_mode_t power_manager_get_mode(const power_manager_t *pm)
{
    if (!pm) return POWER_MODE_NORMAL;
    return pm->mode;
}

/* External RTC handle defined in main.c */
extern RTC_HandleTypeDef g_hrtc;

/**
 * @brief Enter STM32L4 STOP2 mode and wake via RTC alarm
 *
 * In STOP2 mode, the STM32L476 draws ~2 µA.
 * LSE (32.768 kHz) keeps the RTC running for timed wake-up.
 */
void power_manager_enter_stop_mode(uint32_t seconds)
{
    if (seconds == 0) return;

    /* Configure RTC wake-up timer */
    HAL_RTCEx_SetWakeUpTimer_IT(&g_hrtc, seconds - 1,
                                 RTC_WAKEUPCLOCK_CK_SPRE_16BITS);

    /* Suspend SysTick to prevent wake-up */
    HAL_SuspendTick();

    /* Enter STOP2 mode */
    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);

    /* --- MCU resumes here after RTC alarm --- */

    /* Reconfigure system clock (lost in STOP mode) */
    extern void SystemClock_Config(void);
    SystemClock_Config();

    /* Resume SysTick */
    HAL_ResumeTick();

    /* Disable RTC wake-up */
    HAL_RTCEx_DeactivateWakeUpTimer(&g_hrtc);
}

/**
 * @brief Enter standby mode (deep sleep)
 * Wake-up only via external interrupt (e.g. solar voltage rising).
 */
void power_manager_enter_deep_sleep(void)
{
    /* Enable wake-up pin (WKUP1 on PA0) */
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);

    /* Clear wake-up flag */
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

    /* Enter standby mode — all RAM lost, MCU resets on wake */
    HAL_PWR_EnterSTANDBYMode();
}

/**
 * @file system_clock.c
 * @brief System clock configuration for STM32L476RG
 *
 * Configures MSI PLL to 80 MHz. Also configures LSE for RTC.
 */

#include "stm32l4xx_hal.h"

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    RCC_PeriphCLKInitTypeDef periph = {0};

    /* Configure power regulator voltage scale 1 (up to 80 MHz) */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
        while (1);
    }

    /* Enable MSI, HSE not used. Enable LSE for RTC. */
    osc.OscillatorType = RCC_OSCILLATORTYPE_MSI | RCC_OSCILLATORTYPE_LSE;
    osc.MSIState       = RCC_MSI_ON;
    osc.MSICalibrationValue = 0;
    osc.MSIClockRange  = RCC_MSIRANGE_6;   /* 4 MHz MSI */
    osc.LSEState       = RCC_LSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_MSI;
    osc.PLL.PLLM       = 1;                /* 4 MHz / 1 = 4 MHz VCO input */
    osc.PLL.PLLN       = 40;               /* 4 MHz × 40 = 160 MHz VCO */
    osc.PLL.PLLP       = RCC_PLLP_DIV7;
    osc.PLL.PLLQ       = RCC_PLLQ_DIV2;
    osc.PLL.PLLR       = RCC_PLLR_DIV2;    /* 160 / 2 = 80 MHz SYSCLK */

    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        while (1);
    }

    /* Enable MSI auto-calibration with LSE */
    HAL_RCCEx_EnableMSIPLLMode();

    /* Configure bus clocks */
    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                          RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;    /* HCLK = 80 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV1;      /* APB1 = 80 MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;      /* APB2 = 80 MHz */

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) {
        while (1);
    }

    /* Configure RTC clock source = LSE */
    periph.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    periph.RTCClockSelection    = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) {
        while (1);
    }

    /* Configure ADC clock = SYSCLK */
    periph.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    periph.AdcClockSelection    = RCC_ADCCLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) {
        while (1);
    }
}

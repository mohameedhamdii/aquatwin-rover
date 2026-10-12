/**
 * @file stm32l4xx_hal_msp.c
 * @brief HAL MSP (MCU Support Package) — peripheral GPIO/clock init
 *
 * Called automatically by HAL_XXX_Init() functions to set up
 * the GPIO pins and clocks for each peripheral.
 */

#include "stm32l4xx_hal.h"
#include "pin_map.h"

/**
 * @brief Global MSP init (called by HAL_Init)
 */
void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

/**
 * @brief ADC MSP init
 */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        __HAL_RCC_ADC_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitTypeDef gpio = {0};
        gpio.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
        gpio.Pull = GPIO_NOPULL;

        /* pH sensor - PA0 */
        gpio.Pin = PH_SENSOR_GPIO_PIN;
        HAL_GPIO_Init(PH_SENSOR_GPIO_PORT, &gpio);

        /* Turbidity sensor - PA1 */
        gpio.Pin = TURB_SENSOR_GPIO_PIN;
        HAL_GPIO_Init(TURB_SENSOR_GPIO_PORT, &gpio);

        /* DO sensor - PA4 */
        gpio.Pin = DO_SENSOR_GPIO_PIN;
        HAL_GPIO_Init(DO_SENSOR_GPIO_PORT, &gpio);

        /* Battery monitor - PA5 */
        gpio.Pin = BATTERY_GPIO_PIN;
        HAL_GPIO_Init(BATTERY_GPIO_PORT, &gpio);

        /* Solar monitor - PA6 */
        gpio.Pin = SOLAR_GPIO_PIN;
        HAL_GPIO_Init(SOLAR_GPIO_PORT, &gpio);
    }
}

/**
 * @brief SPI MSP init (for LoRa)
 */
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) {
        __HAL_RCC_SPI1_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();

        GPIO_InitTypeDef gpio = {0};
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_NOPULL;
        gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF5_SPI1;

        /* SCK - PB3 */
        gpio.Pin = LORA_SPI_SCK_PIN;
        HAL_GPIO_Init(LORA_SPI_SCK_PORT, &gpio);

        /* MISO - PB4 */
        gpio.Pin = LORA_SPI_MISO_PIN;
        HAL_GPIO_Init(LORA_SPI_MISO_PORT, &gpio);

        /* MOSI - PB5 */
        gpio.Pin = LORA_SPI_MOSI_PIN;
        HAL_GPIO_Init(LORA_SPI_MOSI_PORT, &gpio);

        /* NSS - PB6 (manual, not AF) */
        gpio.Mode = GPIO_MODE_OUTPUT_PP;
        gpio.Alternate = 0;
        gpio.Pin = LORA_NSS_PIN;
        HAL_GPIO_Init(LORA_NSS_PORT, &gpio);
        HAL_GPIO_WritePin(LORA_NSS_PORT, LORA_NSS_PIN, GPIO_PIN_SET);

        /* Reset - PC7 */
        gpio.Pin = LORA_RESET_PIN;
        HAL_GPIO_Init(LORA_RESET_PORT, &gpio);
        HAL_GPIO_WritePin(LORA_RESET_PORT, LORA_RESET_PIN, GPIO_PIN_SET);

        /* DIO0 interrupt - PA9 */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        gpio.Mode = GPIO_MODE_IT_RISING;
        gpio.Pull = GPIO_PULLDOWN;
        gpio.Pin  = LORA_DIO0_PIN;
        HAL_GPIO_Init(LORA_DIO0_PORT, &gpio);

        HAL_NVIC_SetPriority(LORA_DIO0_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(LORA_DIO0_IRQn);
    }
}

/**
 * @brief UART MSP init (GPS + Debug)
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio = {0};

    if (huart->Instance == USART2) {
        /* GPS UART */
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_PULLUP;
        gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio.Alternate = GPIO_AF7_USART2;

        gpio.Pin = GPS_UART_TX_PIN;
        HAL_GPIO_Init(GPS_UART_TX_PORT, &gpio);

        gpio.Pin = GPS_UART_RX_PIN;
        HAL_GPIO_Init(GPS_UART_RX_PORT, &gpio);

        HAL_NVIC_SetPriority(GPS_UART_IRQn, 7, 0);
        HAL_NVIC_EnableIRQ(GPS_UART_IRQn);
    }
    else if (huart->Instance == USART1) {
        /* Debug UART (ST-LINK VCP) */
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_PULLUP;
        gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio.Alternate = GPIO_AF7_USART1;

        gpio.Pin = DEBUG_UART_TX_PIN;
        HAL_GPIO_Init(DEBUG_UART_TX_PORT, &gpio);

        gpio.Pin = DEBUG_UART_RX_PIN;
        HAL_GPIO_Init(DEBUG_UART_RX_PORT, &gpio);
    }
}

/**
 * @brief TIM MSP init (motor PWM)
 */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) {
        __HAL_RCC_TIM1_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();

        GPIO_InitTypeDef gpio = {0};
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_NOPULL;
        gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF1_TIM1;

        gpio.Pin = MOTOR_LEFT_FWD_PIN | MOTOR_LEFT_REV_PIN |
                   MOTOR_RIGHT_FWD_PIN | MOTOR_RIGHT_REV_PIN;
        HAL_GPIO_Init(GPIOC, &gpio);
    }
}

/**
 * @brief RTC MSP init
 */
void HAL_RTC_MspInit(RTC_HandleTypeDef *hrtc)
{
    (void)hrtc;
    __HAL_RCC_RTC_ENABLE();
    HAL_NVIC_SetPriority(RTC_WKUP_IRQn, 8, 0);
    HAL_NVIC_EnableIRQ(RTC_WKUP_IRQn);
}

/**
 * @file stm32l4xx_it.c
 * @brief Interrupt handlers for STM32L476RG
 */

#include "stm32l4xx_hal.h"
#include "comms/lora_driver.h"
#include "nav/gps_driver.h"
#include "pin_map.h"

/* External references (defined in main.c) */
extern lora_t g_lora;
extern UART_HandleTypeDef g_huart_gps;
extern RTC_HandleTypeDef g_hrtc;

#ifdef BUILD_MODE_ROVER
extern gps_driver_t g_gps;
uint8_t g_gps_rx_byte;
#endif

/* ============================================================================
 * Cortex-M4 System Exceptions
 * Note: SVC_Handler, PendSV_Handler, SysTick_Handler are mapped
 * to FreeRTOS in FreeRTOSConfig.h
 * ============================================================================ */

void NMI_Handler(void)
{
    while (1);
}

void HardFault_Handler(void)
{
    while (1);
}

void MemManage_Handler(void)
{
    while (1);
}

void BusFault_Handler(void)
{
    while (1);
}

void UsageFault_Handler(void)
{
    while (1);
}

void DebugMon_Handler(void)
{
}

/* ============================================================================
 * Peripheral Interrupts
 * ============================================================================ */

/**
 * @brief EXTI9_5 — LoRa DIO0 (TX done)
 */
void EXTI9_5_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(LORA_DIO0_PIN) != RESET) {
        __HAL_GPIO_EXTI_CLEAR_IT(LORA_DIO0_PIN);
        lora_dio0_irq_handler(&g_lora);
    }
}

/**
 * @brief USART2 — GPS UART RX
 */
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&g_huart_gps);
}

#ifdef BUILD_MODE_ROVER
/**
 * @brief UART RX complete callback — feed GPS parser and re-arm
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        gps_uart_rx_callback(&g_gps, g_gps_rx_byte);
        HAL_UART_Receive_IT(&g_huart_gps, &g_gps_rx_byte, 1);
    }
}
#endif

/**
 * @brief RTC wake-up timer interrupt
 */
void RTC_WKUP_IRQHandler(void)
{
    HAL_RTCEx_WakeUpTimerIRQHandler(&g_hrtc);
}

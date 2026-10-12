/**
 * @file freertos_hooks.c
 * @brief FreeRTOS hook functions
 */

#include "FreeRTOS.h"
#include "task.h"
#include "stm32l4xx_hal.h"
#include "pin_map.h"

/**
 * @brief Idle hook — blink status LED slowly to indicate alive
 */
void vApplicationIdleHook(void)
{
    /* Low-power idle — WFI until next interrupt */
    __WFI();
}

/**
 * @brief Malloc failed hook — halt with rapid LED blink
 */
void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    while (1) {
        HAL_GPIO_TogglePin(STATUS_LED_PORT, STATUS_LED_PIN);
        for (volatile uint32_t i = 0; i < 200000; i++);
    }
}

/**
 * @brief Stack overflow hook — halt
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    while (1);
}

/**
 * @brief Provide static memory for the idle task
 */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t idle_tcb;
    static StackType_t  idle_stack[configMINIMAL_STACK_SIZE];

    *ppxIdleTaskTCBBuffer   = &idle_tcb;
    *ppxIdleTaskStackBuffer = idle_stack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

/**
 * @brief Provide static memory for the timer task
 */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t timer_tcb;
    static StackType_t  timer_stack[configTIMER_TASK_STACK_DEPTH];

    *ppxTimerTaskTCBBuffer   = &timer_tcb;
    *ppxTimerTaskStackBuffer = timer_stack;
    *pulTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}

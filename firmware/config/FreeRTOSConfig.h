/**
 * @file FreeRTOSConfig.h
 * @brief FreeRTOS configuration for STM32L476RG Water Quality Monitor
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Cortex-M4 specific */
#define configUSE_PREEMPTION                     1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1
#define configUSE_TICKLESS_IDLE                  2   /* Custom tickless for STOP mode */
#define configCPU_CLOCK_HZ                       80000000UL  /* 80 MHz MSI/PLL */
#define configTICK_RATE_HZ                       1000
#define configMAX_PRIORITIES                     8
#define configMINIMAL_STACK_SIZE                 128  /* words (512 bytes) */
#define configMAX_TASK_NAME_LEN                  16
#define configUSE_16_BIT_TICKS                   0
#define configIDLE_SHOULD_YIELD                  1
#define configUSE_TASK_NOTIFICATIONS             1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    3
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            1
#define configQUEUE_REGISTRY_SIZE                8
#define configUSE_QUEUE_SETS                     0
#define configUSE_TIME_SLICING                   1
#define configSTACK_DEPTH_TYPE                   uint16_t
#define configMESSAGE_BUFFER_LENGTH_TYPE         size_t

/* Memory allocation */
#define configSUPPORT_STATIC_ALLOCATION          1
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configTOTAL_HEAP_SIZE                    ((size_t)(32 * 1024))  /* 32 KB */
#define configAPPLICATION_ALLOCATED_HEAP         0

/* Hook functions */
#define configUSE_IDLE_HOOK                      1  /* For low-power sleep */
#define configUSE_TICK_HOOK                      0
#define configUSE_MALLOC_FAILED_HOOK             1
#define configCHECK_FOR_STACK_OVERFLOW           2  /* Method 2 (pattern check) */

/* Runtime stats */
#define configGENERATE_RUN_TIME_STATS            0
#define configUSE_TRACE_FACILITY                 0
#define configUSE_STATS_FORMATTING_FUNCTIONS     0

/* Co-routine */
#define configUSE_CO_ROUTINES                    0
#define configMAX_CO_ROUTINE_PRIORITIES          2

/* Software timer */
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                 10
#define configTIMER_TASK_STACK_DEPTH             256

/* Interrupt priorities (STM32 uses 4 priority bits) */
#define configPRIO_BITS                          4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY          (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY     (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* Assert */
#define configASSERT(x) if((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); }

/* Map FreeRTOS port interrupt handlers to STM32 names */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

/* Thread-safe newlib */
#define configUSE_NEWLIB_REENTRANT              0

/* ============================================================================
 * Task stack sizes (words) and priorities
 * ============================================================================ */
#define TASK_SENSOR_STACK_SIZE      512
#define TASK_SENSOR_PRIORITY        4

#define TASK_EKF_STACK_SIZE         512
#define TASK_EKF_PRIORITY           3

#define TASK_COMMS_STACK_SIZE       512
#define TASK_COMMS_PRIORITY         2

#define TASK_POWER_STACK_SIZE       256
#define TASK_POWER_PRIORITY         1

#define TASK_NAV_STACK_SIZE         512
#define TASK_NAV_PRIORITY           3

#endif /* FREERTOS_CONFIG_H */

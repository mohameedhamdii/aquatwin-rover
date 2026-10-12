/**
 * @file main.c
 * @brief Water Quality Monitor — Main application (STM32L476RG + FreeRTOS)
 *
 * Creates FreeRTOS tasks for:
 *   - Sensor reading (1 Hz)
 *   - EKF fusion + anomaly detection
 *   - LoRa transmission
 *   - Power management
 *   - Navigation (rover mode only)
 */

#include "stm32l4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "pin_map.h"
#include "calibration.h"
#include "lora_config.h"
#include "FreeRTOSConfig.h"

#include "sensors/sensor_manager.h"
#include "ekf/water_quality_ekf.h"
#include "ekf/anomaly_detector.h"
#include "comms/lora_driver.h"
#include "comms/packet.h"
#include "power/power_manager.h"

#ifdef BUILD_MODE_ROVER
#include "nav/gps_driver.h"
#include "nav/waypoint_nav.h"
#include "nav/motor_driver.h"
#endif

/* ============================================================================
 * Global peripheral handles
 * ============================================================================ */
ADC_HandleTypeDef   g_hadc1;
SPI_HandleTypeDef   g_hspi1;
UART_HandleTypeDef  g_huart_gps;
UART_HandleTypeDef  g_huart_debug;
RTC_HandleTypeDef   g_hrtc;
TIM_HandleTypeDef   g_htim1;

/* Global driver instances */
sensor_manager_t    g_sensors;
wq_ekf_t            g_wq_ekf;
lora_t              g_lora;
power_manager_t     g_power;

#ifdef BUILD_MODE_ROVER
gps_driver_t        g_gps;
motor_driver_t      g_motors;
waypoint_nav_t      g_nav;
#endif

/* FreeRTOS synchronisation */
static QueueHandle_t     g_sensor_queue;
static SemaphoreHandle_t g_data_mutex;

/* Shared data (protected by mutex) */
static sensor_data_t     g_latest_data;
static anomaly_result_t  g_latest_anomaly;
static gps_fix_t         g_latest_fix;

/* ============================================================================
 * Forward declarations
 * ============================================================================ */
extern void SystemClock_Config(void);
static void MX_ADC1_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_Init(void);
static void MX_USART1_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM1_Init(void);
static void MX_GPIO_Init(void);

static void task_sensor(void *arg);
static void task_ekf(void *arg);
static void task_comms(void *arg);
static void task_power(void *arg);
#ifdef BUILD_MODE_ROVER
static void task_nav(void *arg);
#endif

/* ============================================================================
 * Peripheral Initialisation
 * ============================================================================ */

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Status LED */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = STATUS_LED_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(STATUS_LED_PORT, &gpio);

#ifdef BUILD_MODE_ROVER
    /* Motor sleep pin */
    gpio.Pin = MOTOR_SLEEP_PIN;
    HAL_GPIO_Init(MOTOR_SLEEP_PORT, &gpio);
#endif
}

static void MX_ADC1_Init(void)
{
    g_hadc1.Instance                   = ADC1;
    g_hadc1.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV1;
    g_hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    g_hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    g_hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    g_hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    g_hadc1.Init.LowPowerAutoWait      = DISABLE;
    g_hadc1.Init.ContinuousConvMode    = DISABLE;
    g_hadc1.Init.NbrOfConversion       = 1;
    g_hadc1.Init.DiscontinuousConvMode = DISABLE;
    g_hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    g_hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    g_hadc1.Init.DMAContinuousRequests = DISABLE;
    g_hadc1.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
    g_hadc1.Init.OversamplingMode      = DISABLE;

    HAL_ADC_Init(&g_hadc1);
}

static void MX_SPI1_Init(void)
{
    g_hspi1.Instance               = SPI1;
    g_hspi1.Init.Mode              = SPI_MODE_MASTER;
    g_hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    g_hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    g_hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    g_hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    g_hspi1.Init.NSS               = SPI_NSS_SOFT;
    g_hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; /* 80/16 = 5 MHz */
    g_hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    g_hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    g_hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    g_hspi1.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;

    HAL_SPI_Init(&g_hspi1);
}

static void MX_USART2_Init(void)
{
    g_huart_gps.Instance          = USART2;
    g_huart_gps.Init.BaudRate     = GPS_UART_BAUDRATE;
    g_huart_gps.Init.WordLength   = UART_WORDLENGTH_8B;
    g_huart_gps.Init.StopBits     = UART_STOPBITS_1;
    g_huart_gps.Init.Parity       = UART_PARITY_NONE;
    g_huart_gps.Init.Mode         = UART_MODE_TX_RX;
    g_huart_gps.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    g_huart_gps.Init.OverSampling = UART_OVERSAMPLING_16;

    HAL_UART_Init(&g_huart_gps);
}

static void MX_USART1_Init(void)
{
    g_huart_debug.Instance          = USART1;
    g_huart_debug.Init.BaudRate     = DEBUG_UART_BAUDRATE;
    g_huart_debug.Init.WordLength   = UART_WORDLENGTH_8B;
    g_huart_debug.Init.StopBits     = UART_STOPBITS_1;
    g_huart_debug.Init.Parity       = UART_PARITY_NONE;
    g_huart_debug.Init.Mode         = UART_MODE_TX_RX;
    g_huart_debug.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    g_huart_debug.Init.OverSampling = UART_OVERSAMPLING_16;

    HAL_UART_Init(&g_huart_debug);
}

static void MX_RTC_Init(void)
{
    g_hrtc.Instance            = RTC;
    g_hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
    g_hrtc.Init.AsynchPrediv   = 127;
    g_hrtc.Init.SynchPrediv    = 255;
    g_hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
    g_hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    g_hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;

    HAL_RTC_Init(&g_hrtc);
}

static void MX_TIM1_Init(void)
{
    g_htim1.Instance               = TIM1;
    g_htim1.Init.Prescaler         = 79;    /* 80 MHz / 80 = 1 MHz timer clock */
    g_htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    g_htim1.Init.Period            = 999;   /* 1 MHz / 1000 = 1 kHz PWM */
    g_htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    g_htim1.Init.RepetitionCounter = 0;
    g_htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    HAL_TIM_PWM_Init(&g_htim1);

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;

    HAL_TIM_PWM_ConfigChannel(&g_htim1, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&g_htim1, &oc, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&g_htim1, &oc, TIM_CHANNEL_3);
    HAL_TIM_PWM_ConfigChannel(&g_htim1, &oc, TIM_CHANNEL_4);
}

/* ============================================================================
 * Debug printf via UART1
 * ============================================================================ */
#include <stdio.h>

int _write(int file, char *ptr, int len)
{
    (void)file;
    HAL_UART_Transmit(&g_huart_debug, (uint8_t *)ptr, (uint16_t)len, 100);
    return len;
}

/* ============================================================================
 * FreeRTOS Tasks
 * ============================================================================ */

/**
 * @brief Sensor reading task (highest priority)
 *
 * Reads all sensors at 1 Hz and posts data to the EKF queue.
 */
static void task_sensor(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    bool first_reading = true;

    for (;;) {
        /* Read all sensors (blocking, ~1 second with oversampling) */
        sensor_data_t data = sensor_manager_read_all(&g_sensors);

        if (data.valid) {
            /* Initialise EKF with first valid reading */
            if (first_reading) {
                wq_ekf_init_from_measurement(&g_wq_ekf, &data);
                first_reading = false;
            }

            /* Post to EKF queue */
            xQueueOverwrite(g_sensor_queue, &data);
        }

        /* Toggle LED to show we're alive */
        HAL_GPIO_TogglePin(STATUS_LED_PORT, STATUS_LED_PIN);

        /* Wait for next cycle (accounts for time spent in read_all) */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief EKF fusion and anomaly detection task
 */
static void task_ekf(void *arg)
{
    (void)arg;
    sensor_data_t data;

    for (;;) {
        /* Wait for new sensor data */
        if (xQueueReceive(g_sensor_queue, &data, portMAX_DELAY) == pdTRUE) {
            /* Run EKF predict-update */
            wq_ekf_update(&g_wq_ekf, &data);

            /* Get filtered data */
            sensor_data_t filtered;
            wq_ekf_get_filtered(&g_wq_ekf, &filtered);

            /* Check for anomalies */
            anomaly_result_t anomaly = anomaly_detector_check(&g_wq_ekf, &filtered);

            /* Update shared data (protected by mutex) */
            if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                g_latest_data = filtered;
                g_latest_anomaly = anomaly;
                xSemaphoreGive(g_data_mutex);
            }
        }
    }
}

/**
 * @brief LoRa communication task
 *
 * Transmits data packets at the configured interval.
 */
static void task_comms(void *arg)
{
    (void)arg;

    for (;;) {
        /* Get sleep duration from power manager */
        uint32_t interval_s = power_manager_get_sleep_duration(&g_power);
        vTaskDelay(pdMS_TO_TICKS(interval_s * 1000));

        /* Grab latest data */
        sensor_data_t data = {0};
        anomaly_result_t anomaly = {0};
        gps_fix_t fix = {0};

        if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            data = g_latest_data;
            anomaly = g_latest_anomaly;
            fix = g_latest_fix;
            xSemaphoreGive(g_data_mutex);
        }

        /* Read power state */
        uint16_t battery_mv = g_power.battery.voltage_mv;
        uint8_t  solar_pct  = g_power.solar.power_pct;

        /* Assemble packet */
        wq_packet_t pkt = packet_assemble(&data, &fix, battery_mv, solar_pct, &anomaly);

        /* Wake radio and transmit */
        lora_standby(&g_lora);
        lora_status_t status = lora_transmit(&g_lora, (uint8_t *)&pkt, sizeof(pkt));
        lora_sleep(&g_lora);

        if (status == LORA_OK) {
            printf("[COMMS] TX OK — pH=%.2f turb=%.1f DO=%.2f T=%.1f bat=%dmV\r\n",
                   data.ph, data.turbidity, data.do_mg_l, data.temp_c, battery_mv);
        } else {
            printf("[COMMS] TX FAIL (err=%d)\r\n", status);
        }

        if (anomaly_is_flagged(&anomaly)) {
            printf("[ALERT] Anomaly flags=0x%02X NIS=%.2f\r\n",
                   anomaly.flags, anomaly.nis);
        }
    }
}

/**
 * @brief Power management task (lowest priority)
 */
static void task_power(void *arg)
{
    (void)arg;

    for (;;) {
        power_manager_update(&g_power);

        power_mode_t mode = power_manager_get_mode(&g_power);

        if (mode == POWER_MODE_CRITICAL) {
            printf("[POWER] CRITICAL — entering deep sleep\r\n");
            HAL_Delay(100);  /* Flush UART */

#ifdef BUILD_MODE_ROVER
            motor_sleep(&g_motors);
#endif
            lora_sleep(&g_lora);
            power_manager_enter_deep_sleep();
            /* MCU resets on wake from standby */
        }

        printf("[POWER] bat=%dmV (%d%%) solar=%dmV mode=%s\r\n",
               g_power.battery.voltage_mv, g_power.battery.soc_pct,
               g_power.solar.voltage_mv,
               mode == POWER_MODE_NORMAL ? "NORMAL" : "LOW_POWER");

        vTaskDelay(pdMS_TO_TICKS(10000));  /* Check every 10s */
    }
}

#ifdef BUILD_MODE_ROVER
/**
 * @brief Navigation task — waypoint patrol
 */
static void task_nav(void *arg)
{
    (void)arg;

    for (;;) {
        /* Process incoming GPS data */
        gps_process(&g_gps);

        /* Update shared GPS fix */
        gps_fix_t fix = gps_get_fix(&g_gps);
        if (fix.fix_valid) {
            if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                g_latest_fix = fix;
                xSemaphoreGive(g_data_mutex);
            }
        }

        /* Update navigation */
        waypoint_nav_update(&g_nav);

        vTaskDelay(pdMS_TO_TICKS(200));  /* 5 Hz nav update */
    }
}
#endif

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    /* HAL and clock init */
    HAL_Init();
    SystemClock_Config();

    /* Peripheral init */
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_SPI1_Init();
    MX_USART1_Init();    /* Debug UART */
    MX_USART2_Init();    /* GPS UART */
    MX_RTC_Init();
    MX_TIM1_Init();

    printf("\r\n========================================\r\n");
    printf("  Water Quality Monitor v1.0\r\n");
    printf("  STM32L476RG + FreeRTOS\r\n");
#ifdef BUILD_MODE_ROVER
    printf("  Mode: ROVER\r\n");
#else
    printf("  Mode: BUOY\r\n");
#endif
    printf("========================================\r\n\r\n");

    /* Initialise drivers */
    printf("[INIT] Sensors...\r\n");
    sensor_manager_init(&g_sensors, &g_hadc1);

    printf("[INIT] EKF...\r\n");
    wq_ekf_init(&g_wq_ekf);

    printf("[INIT] LoRa...\r\n");
    lora_status_t lora_rc = lora_init(&g_lora, &g_hspi1);
    if (lora_rc != LORA_OK) {
        printf("[ERROR] LoRa init failed (err=%d)\r\n", lora_rc);
    } else {
        printf("[INIT] LoRa OK — %lu MHz, SF%d\r\n",
               LORA_FREQUENCY_HZ / 1000000, LORA_SPREADING_FACTOR);
    }

    printf("[INIT] Power manager...\r\n");
    power_manager_init(&g_power, &g_hadc1);

#ifdef BUILD_MODE_ROVER
    printf("[INIT] GPS...\r\n");
    gps_init(&g_gps, &g_huart_gps);

    printf("[INIT] Motors...\r\n");
    motor_driver_init(&g_motors, &g_htim1);

    printf("[INIT] Navigation...\r\n");
    waypoint_nav_init(&g_nav, &g_gps, &g_motors);

    /* Example patrol waypoints (replace with actual coordinates) */
    waypoint_nav_add_waypoint(&g_nav, 515000000, -1280000);   /* 51.5°N, 0.128°W */
    waypoint_nav_add_waypoint(&g_nav, 515001000, -1279000);
    waypoint_nav_add_waypoint(&g_nav, 515002000, -1278000);

    /* Start GPS UART interrupt reception */
    static uint8_t gps_rx_byte;
    HAL_UART_Receive_IT(&g_huart_gps, &gps_rx_byte, 1);

    waypoint_nav_start(&g_nav);
#endif

    /* Create FreeRTOS objects */
    g_sensor_queue = xQueueCreate(1, sizeof(sensor_data_t));
    g_data_mutex   = xSemaphoreCreateMutex();

    /* Create tasks */
    xTaskCreate(task_sensor, "Sensor", TASK_SENSOR_STACK_SIZE, NULL,
                TASK_SENSOR_PRIORITY, NULL);
    xTaskCreate(task_ekf, "EKF", TASK_EKF_STACK_SIZE, NULL,
                TASK_EKF_PRIORITY, NULL);
    xTaskCreate(task_comms, "Comms", TASK_COMMS_STACK_SIZE, NULL,
                TASK_COMMS_PRIORITY, NULL);
    xTaskCreate(task_power, "Power", TASK_POWER_STACK_SIZE, NULL,
                TASK_POWER_PRIORITY, NULL);

#ifdef BUILD_MODE_ROVER
    xTaskCreate(task_nav, "Nav", TASK_NAV_STACK_SIZE, NULL,
                TASK_NAV_PRIORITY, NULL);
#endif

    printf("[INIT] Starting FreeRTOS scheduler\r\n");

    /* Start scheduler — never returns */
    vTaskStartScheduler();

    /* Should never reach here */
    while (1);
}

/* HAL requires this */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    (void)htim;
}

/* stm32l4xx_hal_conf.h assert macro */
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    printf("ASSERT FAILED: %s:%lu\r\n", (char *)file, line);
    while (1);
}
#endif

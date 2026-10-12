/**
 * @file temperature_sensor.c
 * @brief DS18B20 OneWire temperature sensor driver implementation
 *
 * Bit-banged OneWire protocol using a hardware timer for microsecond delays.
 * Uses the DS18B20 in single-device mode (skip ROM addressing).
 */

#include "sensors/temperature_sensor.h"
#include "pin_map.h"
#include "stm32l4xx_hal.h"

/* ============================================================================
 * OneWire timing constants (microseconds)
 * ============================================================================ */
#define OW_RESET_PULSE_US       480
#define OW_PRESENCE_WAIT_US     70
#define OW_PRESENCE_TIMEOUT_US  240
#define OW_WRITE_SLOT_US        60
#define OW_WRITE_1_LOW_US       6
#define OW_WRITE_0_LOW_US       60
#define OW_READ_SLOT_US         60
#define OW_READ_SAMPLE_US       9
#define OW_RECOVERY_US          10

/* DS18B20 commands */
#define DS18B20_CMD_CONVERT_T   0x44
#define DS18B20_CMD_READ_SCRATCH 0xBE
#define DS18B20_CMD_SKIP_ROM    0xCC

/* ============================================================================
 * Microsecond delay using DWT cycle counter
 * ============================================================================ */
static void delay_us(uint32_t us)
{
    uint32_t clk_mhz = HAL_RCC_GetHCLKFreq() / 1000000;
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * clk_mhz;
    while ((DWT->CYCCNT - start) < ticks);
}

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* ============================================================================
 * OneWire GPIO helpers
 * ============================================================================ */
static void ow_pin_output(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = DS18B20_GPIO_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;  /* Open-drain for OneWire */
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DS18B20_GPIO_PORT, &gpio);
}

static void ow_pin_input(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = DS18B20_GPIO_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DS18B20_GPIO_PORT, &gpio);
}

static void ow_low(void)
{
    HAL_GPIO_WritePin(DS18B20_GPIO_PORT, DS18B20_GPIO_PIN, GPIO_PIN_RESET);
}

static void ow_high(void)
{
    HAL_GPIO_WritePin(DS18B20_GPIO_PORT, DS18B20_GPIO_PIN, GPIO_PIN_SET);
}

static uint8_t ow_read_pin(void)
{
    return (HAL_GPIO_ReadPin(DS18B20_GPIO_PORT, DS18B20_GPIO_PIN) == GPIO_PIN_SET) ? 1 : 0;
}

/* ============================================================================
 * OneWire protocol
 * ============================================================================ */

/**
 * @brief Send reset pulse and detect presence
 * @return 1 if device present, 0 if not
 */
static int ow_reset(void)
{
    ow_pin_output();
    ow_low();
    delay_us(OW_RESET_PULSE_US);
    ow_high();
    ow_pin_input();
    delay_us(OW_PRESENCE_WAIT_US);

    int present = !ow_read_pin();  /* Low = device present */
    delay_us(OW_PRESENCE_TIMEOUT_US);

    return present;
}

static void ow_write_bit(uint8_t bit)
{
    ow_pin_output();
    if (bit) {
        ow_low();
        delay_us(OW_WRITE_1_LOW_US);
        ow_high();
        delay_us(OW_WRITE_SLOT_US - OW_WRITE_1_LOW_US);
    } else {
        ow_low();
        delay_us(OW_WRITE_0_LOW_US);
        ow_high();
    }
    delay_us(OW_RECOVERY_US);
}

static uint8_t ow_read_bit(void)
{
    ow_pin_output();
    ow_low();
    delay_us(3);
    ow_high();
    ow_pin_input();
    delay_us(OW_READ_SAMPLE_US);
    uint8_t bit = ow_read_pin();
    delay_us(OW_READ_SLOT_US - OW_READ_SAMPLE_US);
    return bit;
}

static void ow_write_byte(uint8_t data)
{
    for (int i = 0; i < 8; i++) {
        ow_write_bit(data & 0x01);
        data >>= 1;
    }
}

static uint8_t ow_read_byte(void)
{
    uint8_t data = 0;
    for (int i = 0; i < 8; i++) {
        data >>= 1;
        if (ow_read_bit()) {
            data |= 0x80;
        }
    }
    return data;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int temp_sensor_init(temp_sensor_t *ctx)
{
    if (!ctx) return -1;

    /* Enable GPIO clock for DS18B20 pin */
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Initialise DWT for microsecond delays */
    dwt_init();

    /* Check if device is present */
    if (!ow_reset()) {
        ctx->initialised = false;
        return -1;  /* No device found */
    }

    ctx->last_temp_c = 25.0f;
    ctx->conversion_started = false;
    ctx->initialised = true;

    return 0;
}

int temp_sensor_start_conversion(temp_sensor_t *ctx)
{
    if (!ctx || !ctx->initialised) return -1;

    if (!ow_reset()) return -1;

    ow_write_byte(DS18B20_CMD_SKIP_ROM);
    ow_write_byte(DS18B20_CMD_CONVERT_T);

    ctx->conversion_started = true;
    return 0;
}

float temp_sensor_read(temp_sensor_t *ctx)
{
    if (!ctx || !ctx->initialised || !ctx->conversion_started) return -999.0f;

    if (!ow_reset()) return -999.0f;

    ow_write_byte(DS18B20_CMD_SKIP_ROM);
    ow_write_byte(DS18B20_CMD_READ_SCRATCH);

    /* Read 9 bytes of scratchpad */
    uint8_t scratchpad[9];
    for (int i = 0; i < 9; i++) {
        scratchpad[i] = ow_read_byte();
    }

    /* CRC check (byte 8 is CRC) */
    uint8_t crc = 0;
    for (int i = 0; i < 8; i++) {
        uint8_t byte = scratchpad[i];
        for (int j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            byte >>= 1;
        }
    }
    if (crc != scratchpad[8]) {
        return -999.0f;  /* CRC error */
    }

    /* Convert raw temperature (12-bit, 0.0625°C resolution) */
    int16_t raw = (int16_t)((scratchpad[1] << 8) | scratchpad[0]);
    float temp_c = raw * 0.0625f;

    ctx->last_temp_c = temp_c;
    ctx->conversion_started = false;

    return temp_c;
}

float temp_sensor_get_last(const temp_sensor_t *ctx)
{
    if (!ctx || !ctx->initialised) return -999.0f;
    return ctx->last_temp_c;
}

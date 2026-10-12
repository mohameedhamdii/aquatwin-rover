/**
 * @file satellite.c
 * @brief Iridium SBD satellite backhaul driver implementation
 *
 * Communicates with the RockBLOCK 9603 modem via AT commands over UART.
 * Used as a fallback when LoRa gateway is out of range.
 */

#include "comms/satellite.h"
#include <string.h>
#include <stdio.h>

#ifdef ENABLE_SATELLITE

#define SAT_RESPONSE_BUF_SIZE  128
#define SAT_TIMEOUT_MS         30000
#define SAT_CMD_TIMEOUT_MS     5000

static uint8_t resp_buf[SAT_RESPONSE_BUF_SIZE];

/**
 * @brief Send an AT command and wait for response
 */
static sat_status_t sat_send_at(satellite_t *sat, const char *cmd,
                                 const char *expected, uint32_t timeout_ms)
{
    uint16_t len = (uint16_t)strlen(cmd);
    HAL_UART_Transmit(sat->huart, (uint8_t *)cmd, len, 1000);
    HAL_UART_Transmit(sat->huart, (uint8_t *)"\r", 1, 100);

    memset(resp_buf, 0, SAT_RESPONSE_BUF_SIZE);
    HAL_UART_Receive(sat->huart, resp_buf, SAT_RESPONSE_BUF_SIZE - 1, timeout_ms);

    if (expected && strstr((char *)resp_buf, expected) == NULL) {
        return SAT_ERR_SEND_FAIL;
    }

    return SAT_OK;
}

sat_status_t satellite_init(satellite_t *sat, UART_HandleTypeDef *huart)
{
    if (!sat || !huart) return SAT_ERR_INIT;

    sat->huart = huart;
    sat->signal_quality = 0;
    sat->initialised = false;

    /* Check modem is responding */
    if (sat_send_at(sat, "AT", "OK", SAT_CMD_TIMEOUT_MS) != SAT_OK) {
        return SAT_ERR_INIT;
    }

    /* Disable echo */
    sat_send_at(sat, "ATE0", "OK", SAT_CMD_TIMEOUT_MS);

    /* Enable SBD ring alerts */
    sat_send_at(sat, "AT+SBDMTA=1", "OK", SAT_CMD_TIMEOUT_MS);

    sat->initialised = true;
    return SAT_OK;
}

sat_status_t satellite_send(satellite_t *sat, const uint8_t *data, uint8_t length)
{
    if (!sat || !sat->initialised || !data || length == 0) {
        return SAT_ERR_INIT;
    }

    /* Write binary message to MO buffer */
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+SBDWB=%d", length);
    if (sat_send_at(sat, cmd, "READY", SAT_CMD_TIMEOUT_MS) != SAT_OK) {
        return SAT_ERR_SEND_FAIL;
    }

    /* Send binary data + 2-byte checksum */
    uint16_t checksum = 0;
    for (int i = 0; i < length; i++) {
        checksum += data[i];
    }

    HAL_UART_Transmit(sat->huart, (uint8_t *)data, length, 5000);
    uint8_t cs[2] = { (uint8_t)(checksum >> 8), (uint8_t)(checksum & 0xFF) };
    HAL_UART_Transmit(sat->huart, cs, 2, 1000);

    /* Wait for write confirmation */
    memset(resp_buf, 0, SAT_RESPONSE_BUF_SIZE);
    HAL_UART_Receive(sat->huart, resp_buf, SAT_RESPONSE_BUF_SIZE - 1, SAT_CMD_TIMEOUT_MS);

    /* Initiate SBD session */
    if (sat_send_at(sat, "AT+SBDIX", "+SBDIX:", SAT_TIMEOUT_MS) != SAT_OK) {
        return SAT_ERR_TIMEOUT;
    }

    /* Parse response: +SBDIX: <MO status>, ... */
    char *p = strstr((char *)resp_buf, "+SBDIX:");
    if (p) {
        int mo_status = 0;
        sscanf(p, "+SBDIX: %d", &mo_status);
        if (mo_status <= 2) {
            return SAT_OK;  /* Successful transfer */
        }
    }

    return SAT_ERR_SEND_FAIL;
}

uint8_t satellite_get_signal(satellite_t *sat)
{
    if (!sat || !sat->initialised) return 0;

    if (sat_send_at(sat, "AT+CSQ", "+CSQ:", SAT_CMD_TIMEOUT_MS) == SAT_OK) {
        char *p = strstr((char *)resp_buf, "+CSQ:");
        if (p) {
            int sig = 0;
            sscanf(p, "+CSQ:%d", &sig);
            sat->signal_quality = (uint8_t)sig;
        }
    }

    return sat->signal_quality;
}

void satellite_sleep(satellite_t *sat)
{
    if (!sat || !sat->initialised) return;
    /* The RockBLOCK doesn't have a software sleep — control via power MOSFET */
}

#endif /* ENABLE_SATELLITE */

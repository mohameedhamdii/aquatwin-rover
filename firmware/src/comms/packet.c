/**
 * @file packet.c
 * @brief Packet assembly and parsing implementation
 */

#include "comms/packet.h"
#include "lora_config.h"
#include <string.h>

wq_packet_t packet_assemble(const sensor_data_t *data,
                             const gps_fix_t *fix,
                             uint16_t battery_mv,
                             uint8_t solar_pct,
                             const anomaly_result_t *anomaly)
{
    wq_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));

    /* GPS data */
    if (fix && fix->fix_valid) {
        pkt.timestamp = fix->timestamp;
        pkt.latitude  = fix->latitude;
        pkt.longitude = fix->longitude;
    } else {
        pkt.timestamp = 0;
        pkt.latitude  = 0;
        pkt.longitude = 0;
    }

    /* Sensor data — convert float to fixed-point */
    if (data) {
        pkt.ph_x100       = (int16_t)(data->ph * 100.0f);
        pkt.turbidity_x10 = (int16_t)(data->turbidity * 10.0f);
        pkt.do_x100       = (int16_t)(data->do_mg_l * 100.0f);
        pkt.temp_x100     = (int16_t)(data->temp_c * 100.0f);
    }

    /* Power data */
    pkt.battery_mv     = battery_mv;
    pkt.solar_power_pct = solar_pct;

    /* Anomaly data */
    if (anomaly) {
        pkt.anomaly_flags = anomaly->flags;
        /* Clamp NIS to uint16 range */
        float nis_100 = anomaly->nis * 100.0f;
        if (nis_100 > 65535.0f) nis_100 = 65535.0f;
        if (nis_100 < 0.0f)    nis_100 = 0.0f;
        pkt.ekf_nis_x100 = (uint16_t)nis_100;
    }

    /* Metadata */
    pkt.device_id      = LORA_DEVICE_ID;
    pkt.packet_version = WQ_PACKET_VERSION;

    return pkt;
}

void packet_parse(const wq_packet_t *pkt, sensor_data_t *data, gps_fix_t *fix)
{
    if (!pkt) return;

    if (data) {
        data->ph        = (float)pkt->ph_x100 / 100.0f;
        data->turbidity = (float)pkt->turbidity_x10 / 10.0f;
        data->do_mg_l   = (float)pkt->do_x100 / 100.0f;
        data->temp_c    = (float)pkt->temp_x100 / 100.0f;
        data->valid     = true;
    }

    if (fix) {
        fix->timestamp  = pkt->timestamp;
        fix->latitude   = pkt->latitude;
        fix->longitude  = pkt->longitude;
        fix->fix_valid  = (pkt->latitude != 0 || pkt->longitude != 0) ? 1 : 0;
    }
}

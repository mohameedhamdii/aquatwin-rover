/**
 * @file test_packet.c
 * @brief Unit tests for LoRa binary telemetry packet assembly and parsing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "comms/packet.h"
#include "sensors/sensor_manager.h"
#include "ekf/anomaly_detector.h"

#define ASSERT_NEAR(val, target, eps) \
    do { \
        float diff = fabsf((float)(val) - (float)(target)); \
        if (diff > (eps)) { \
            fprintf(stderr, "FAIL: %s:%d: Expected %f ~= %f (diff=%f > %f)\n", \
                    __FILE__, __LINE__, (double)(val), (double)(target), (double)diff, (double)(eps)); \
            exit(1); \
        } \
    } while(0)

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s:%d: Condition failed: %s\n", __FILE__, __LINE__, #cond); \
            exit(1); \
        } \
    } while(0)

static void test_packet_structure_size(void)
{
    printf("[TEST] Packet size and alignment verification...\n");
    printf("  sizeof(wq_packet_t) = %zu bytes (must be exactly 32)\n", sizeof(wq_packet_t));
    ASSERT_TRUE(sizeof(wq_packet_t) == 32);
    printf("  PASS: Packet conforms strictly to 32-byte payload budget\n");
}

static void test_packet_assembly_and_parsing(void)
{
    printf("[TEST] Packet assembly, fixed-point encoding and parsing...\n");

    sensor_data_t sensor_in = {
        .ph = 7.35f,
        .turbidity = 14.8f,
        .do_mg_l = 8.42f,
        .temp_c = 19.85f,
        .valid = true,
        .timestamp_ms = 1698000000
    };

    gps_fix_t gps_in = {
        .latitude = 515074000,    /* 51.5074 N */
        .longitude = -1278000,    /* -0.1278 W */
        .timestamp = 1698000000,
        .fix_valid = 1
    };

    anomaly_result_t anomaly_in = {
        .flags = ANOMALY_FLAG_PH | ANOMALY_FLAG_NIS,
        .nis = 14.50f
    };

    uint16_t battery_mv = 3850;
    uint8_t solar_pct = 75;

    wq_packet_t pkt = packet_assemble(&sensor_in, &gps_in, battery_mv, solar_pct, &anomaly_in);

    /* Verify packet header and fields */
    ASSERT_TRUE(pkt.packet_version == WQ_PACKET_VERSION);
    ASSERT_TRUE(pkt.latitude == 515074000);
    ASSERT_TRUE(pkt.longitude == -1278000);
    ASSERT_TRUE(pkt.battery_mv == 3850);
    ASSERT_TRUE(pkt.solar_power_pct == 75);
    ASSERT_TRUE(pkt.anomaly_flags == (ANOMALY_FLAG_PH | ANOMALY_FLAG_NIS));
    ASSERT_TRUE(pkt.ph_x100 == 735);
    ASSERT_TRUE(pkt.turbidity_x10 == 148);
    ASSERT_TRUE(pkt.do_x100 == 842);
    ASSERT_TRUE(pkt.temp_x100 == 1985);
    ASSERT_TRUE(pkt.ekf_nis_x100 == 1450);

    /* Parse back */
    sensor_data_t sensor_out;
    gps_fix_t gps_out;
    packet_parse(&pkt, &sensor_out, &gps_out);

    ASSERT_NEAR(sensor_out.ph, 7.35f, 0.015f);
    ASSERT_NEAR(sensor_out.turbidity, 14.8f, 0.15f);
    ASSERT_NEAR(sensor_out.do_mg_l, 8.42f, 0.015f);
    ASSERT_NEAR(sensor_out.temp_c, 19.85f, 0.015f);
    ASSERT_TRUE(gps_out.latitude == 515074000);
    ASSERT_TRUE(gps_out.longitude == -1278000);

    printf("  PASS: Round-trip fixed-point encoding/decoding is lossless within resolution bounds\n");
}

int main(void)
{
    printf("=== RUNNING PACKET ENCODING/DECODING UNIT TESTS ===\n");
    test_packet_structure_size();
    test_packet_assembly_and_parsing();
    printf("=== ALL PACKET TESTS PASSED SUCCESSFULLY ===\n\n");
    return 0;
}

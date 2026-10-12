# LoRa RF Network Architecture & Gateway Setup Guide

This guide describes the long-range radio configuration, payload format, duty-cycle compliance, and gateway integrations for the AquaTwin field telemetry system.

---

## 1. Frequency Bands & Regional Profiles

| Region | Frequency Band | Channel Spacing | Max EIRP | Duty Cycle Limitation |
|---|---|---|---|---|
| **Europe (EU)** | `868.1 – 868.5 MHz` | 125 kHz | +14 dBm (25 mW) | 1.0% (max 36 seconds airtime / hour) |
| **North America (US)**| `902.3 – 914.9 MHz` | 125 / 500 kHz | +30 dBm (1 W) | Frequency hopping required, no % cap |
| **Asia / Australasia**| `923.0 – 925.0 MHz` | 125 kHz | +16 dBm | Dwell time limitation (400 ms) |

Configure the region flag in `firmware/CMakeLists.txt`:
```cmake
option(LORA_REGION_EU868 "Use 868 MHz LoRa (EU)" ON)
```

---

## 2. Radio Modulation Parameters

- **Modulation:** Chirp Spread Spectrum (CSS)
- **Spreading Factor (SF):** `SF10` (Default)
  - Provides reliable range up to 10–12 km line-of-sight over water surfaces.
  - Time on Air for 32 bytes @ SF10/125kHz: **~370 ms**.
  - At a 60-second transmission interval, total airtime is ~22.2 seconds/hour (compliant with the EU 1% duty cycle limit of 36 s/h).
- **Bandwidth (BW):** `125 kHz`
- **Coding Rate (CR):** `4/5` (1 error correction bit per 4 data bits)
- **Preamble Length:** 8 symbols
- **Sync Word:** `0x12` (Private LoRa network) or `0x34` (Public LoRaWAN)

---

## 3. Binary Packet Format (32 Bytes Fixed Payload)

The telemetry frame is packed into an exact 32-byte binary struct to maximize radio link budget and conserve battery:

| Offset | Type | Field Name | Unit / Scaling | Description |
|---|---|---|---|---|
| `0` | `uint32_t` | `timestamp` | Seconds | Unix epoch timestamp |
| `4` | `int32_t` | `latitude` | Deg × 1e7 | e.g. `515074000` = 51.5074° N |
| `8` | `int32_t` | `longitude` | Deg × 1e7 | e.g. `-1278000` = -0.1278° W |
| `12` | `int16_t` | `ph_x100` | pH × 100 | e.g. `735` = 7.35 pH |
| `14` | `int16_t` | `turbidity_x10`| NTU × 10 | e.g. `85` = 8.5 NTU |
| `16` | `int16_t` | `do_x100` | mg/L × 100 | e.g. `842` = 8.42 mg/L |
| `18` | `int16_t` | `temp_x100` | °C × 100 | e.g. `1950` = 19.50 °C |
| `20` | `uint16_t` | `battery_mv` | Millivolts | e.g. `3940` = 3.94 V |
| `22` | `uint8_t` | `solar_power_pct`| % | 0–100% solar generation |
| `23` | `uint8_t` | `anomaly_flags`| Bitmask | `0x01`:pH, `0x02`:Turb, `0x04`:DO, `0x08`:Temp, `0x10`:NIS |
| `24` | `uint16_t` | `ekf_nis_x100` | NIS × 100 | Normalized Innovation Squared |
| `26` | `uint8_t` | `device_id` | Integer | Unique rover/buoy hardware ID |
| `27` | `uint8_t` | `packet_version`| Integer | Protocol version (`0x01`) |
| `28` | `uint8_t[4]`| `reserved` | Bytes | Reserved for future telemetry |

---

## 4. The Things Network (TTN) / ChirpStack JavaScript Payload Decoder

If deploying via a public or private LoRaWAN network server (such as TTN or ChirpStack), paste the following decoder function into the application Payload Formatter:

```javascript
function decodeUplink(input) {
    var bytes = input.bytes;
    if (bytes.length < 32) {
        return { errors: ["Invalid payload length"] };
    }

    var view = new DataView(new Uint8Array(bytes).buffer);
    var timestamp = view.getUint32(0, true);
    var latitude = view.getInt32(4, true) / 1e7;
    var longitude = view.getInt32(8, true) / 1e7;
    var ph = view.getInt16(12, true) / 100.0;
    var turbidity = view.getInt16(14, true) / 10.0;
    var do_mg_l = view.getInt16(16, true) / 100.0;
    var temp_c = view.getInt16(18, true) / 100.0;
    var battery_mv = view.getUint16(20, true);
    var solar_pct = view.getUint8(22);
    var anomaly_flags = view.getUint8(23);
    var ekf_nis = view.getUint16(24, true) / 100.0;
    var device_id = view.getUint8(26);

    return {
        data: {
            timestamp: timestamp,
            device_id: device_id,
            latitude: latitude,
            longitude: longitude,
            ph: ph,
            turbidity: turbidity,
            do_mg_l: do_mg_l,
            temp_c: temp_c,
            battery_mv: battery_mv,
            solar_power_pct: solar_pct,
            anomaly_flags: anomaly_flags,
            ekf_nis: ekf_nis,
            is_anomaly: (anomaly_flags !== 0)
        }
    };
}
```

---

## 5. Gateway Hardware Options

1. **Option A: RAK7268 Commercial LoRaWAN Gateway**
   - High-grade outdoor IP67 gateway with PoE.
   - Configure HTTP Integration to forward uplinks to:
     `http://<server-ip>:8000/api/v1/data`
2. **Option B: Raspberry Pi + RAK2245 LoRa HAT**
   - Connect SPI HAT on 40-pin header.
   - Run packet forwarder or `gateway/lora_receiver.py`.
3. **Option C: USB Serial LoRa Receiver (Quick Dev Setup)**
   - Connect an SX1276 receiver MCU via USB-to-UART adapter.
   - Run:
     ```bash
     python gateway/lora_receiver.py --config config.yaml
     ```

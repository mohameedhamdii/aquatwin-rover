"""
Core Data Processing Service.
Handles binary packet deserialisation, state calculation, anomaly classification,
WebSocket streaming, and background telemetry generation for simulation.
"""

import struct
import time
import math
import asyncio
import uuid
import logging
from typing import Dict, List, Set, Optional, Any
from fastapi import WebSocket

from backend.api.models import WaterQualityReading, AnomalyAlert, Waypoint, SystemStatus
from backend.services.influx_client import InfluxStorageClient

logger = logging.getLogger("waterquality.service")

# struct format: < I i i h h h h H B B H B B 4s  (32 bytes)
PACKET_STRUCT_FMT = "<Iii4hHBBHBB4s"
PACKET_SIZE = 32

# Bitmasks matching firmware/src/ekf/anomaly_detector.h
FLAG_PH   = (1 << 0)
FLAG_TURB = (1 << 1)
FLAG_DO   = (1 << 2)
FLAG_TEMP = (1 << 3)
FLAG_NIS  = (1 << 4)

def estimate_battery_soc(mv: int) -> float:
    """Approximate LiPo state of charge from millivolts."""
    if mv >= 4200:
        return 100.0
    elif mv <= 3300:
        return 0.0
    elif mv >= 3900:
        return 70.0 + (mv - 3900) * (30.0 / 300.0)
    elif mv >= 3700:
        return 20.0 + (mv - 3700) * (50.0 / 200.0)
    else:
        return 0.0 + (mv - 3300) * (20.0 / 400.0)

class DataService:
    def __init__(self, config: Dict[str, Any], storage: InfluxStorageClient):
        self.config = config
        self.storage = storage
        self.start_time = time.time()
        self.total_packets = 0
        self.latest_reading: Optional[WaterQualityReading] = None
        self.websocket_subscribers: Set[WebSocket] = set()

        # Default Waypoint Patrol Route for Rover Mode
        self.waypoints: List[Waypoint] = [
            Waypoint(id=0, latitude=51.5074, longitude=-0.1278, label="Base Dock", target_reached=True),
            Waypoint(id=1, latitude=51.5085, longitude=-0.1265, label="North Inflow Zone", target_reached=False),
            Waypoint(id=2, latitude=51.5092, longitude=-0.1248, label="Agricultural Runoff Outfall", target_reached=False),
            Waypoint(id=3, latitude=51.5078, longitude=-0.1235, label="Deep Center Basin", target_reached=False),
            Waypoint(id=4, latitude=51.5065, longitude=-0.1252, label="South Spillway", target_reached=False),
        ]
        self.current_waypoint_idx = 0

        # Load most recent real hardware reading from database if available
        stored_readings = self.storage.query_telemetry(limit=1)
        if stored_readings:
            self.latest_reading = stored_readings[0]
            logger.info(f"Loaded latest verified hardware reading from timestamp {self.latest_reading.timestamp}")
        else:
            self.latest_reading = None
            logger.info("Real-time hardware mode: awaiting first telemetry transmission from field node.")

    def decode_binary_packet(self, raw_bytes: bytes) -> WaterQualityReading:
        """
        Unpack the 32-byte binary wq_packet_t structure sent by the STM32/ESP32 firmware.
        """
        if len(raw_bytes) != PACKET_SIZE:
            raise ValueError(f"Packet must be exactly {PACKET_SIZE} bytes (received {len(raw_bytes)})")

        (
            ts,
            lat_int,
            lon_int,
            ph_x100,
            turb_x10,
            do_x100,
            temp_x100,
            batt_mv,
            solar_pct,
            flags,
            nis_x100,
            device_id,
            pkt_ver,
            _reserved
        ) = struct.unpack(PACKET_STRUCT_FMT, raw_bytes)

        # Unscale fixed-point values
        latitude = lat_int / 1e7
        longitude = lon_int / 1e7
        ph = ph_x100 / 100.0
        turbidity = turb_x10 / 10.0
        do_mg_l = do_x100 / 100.0
        temp_c = temp_x100 / 100.0
        ekf_nis = nis_x100 / 100.0
        battery_pct = estimate_battery_soc(batt_mv)

        anomaly_names = []
        if flags & FLAG_PH:
            anomaly_names.append("Abnormal pH Level")
        if flags & FLAG_TURB:
            anomaly_names.append("High Turbidity Plume")
        if flags & FLAG_DO:
            anomaly_names.append("Hypoxia (Low DO)")
        if flags & FLAG_TEMP:
            anomaly_names.append("Thermal Deviation")
        if flags & FLAG_NIS:
            anomaly_names.append("Statistical NIS Divergence")

        is_anomaly = (flags != 0)

        reading = WaterQualityReading(
            timestamp=ts if ts > 0 else int(time.time()),
            device_id=device_id,
            latitude=latitude,
            longitude=longitude,
            ph=round(ph, 2),
            turbidity=round(turbidity, 1),
            do_mg_l=round(do_mg_l, 2),
            temp_c=round(temp_c, 2),
            battery_mv=batt_mv,
            battery_pct=round(battery_pct, 1),
            solar_power_pct=solar_pct,
            anomaly_flags=flags,
            ekf_nis=round(ekf_nis, 2),
            is_anomaly=is_anomaly,
            anomaly_types=anomaly_names
        )
        return reading

    def encode_binary_packet(self, reading: WaterQualityReading) -> bytes:
        """
        Pack a WaterQualityReading into the 32-byte binary packet.
        Useful for simulations, tests, and mock transmissions.
        """
        lat_int = int(round(reading.latitude * 1e7))
        lon_int = int(round(reading.longitude * 1e7))
        ph_x100 = int(round(reading.ph * 100))
        turb_x10 = int(round(reading.turbidity * 10))
        do_x100 = int(round(reading.do_mg_l * 100))
        temp_x100 = int(round(reading.temp_c * 100))
        nis_x100 = int(round(reading.ekf_nis * 100))

        return struct.pack(
            PACKET_STRUCT_FMT,
            reading.timestamp,
            lat_int,
            lon_int,
            ph_x100,
            turb_x10,
            do_x100,
            temp_x100,
            reading.battery_mv,
            reading.solar_power_pct,
            reading.anomaly_flags,
            nis_x100,
            reading.device_id,
            1, # version
            b"\x00\x00\x00\x00"
        )

    async def ingest_reading(self, reading: WaterQualityReading):
        """Process, persist, and broadcast an ingested telemetry reading."""
        self.total_packets += 1
        self.latest_reading = reading

        # Persist reading
        self.storage.write_reading(reading)

        # Check if an anomaly alert needs to be dispatched
        if reading.is_anomaly:
            severity = "CRITICAL" if (reading.anomaly_flags & (FLAG_DO | FLAG_PH)) else "WARNING"
            alert = AnomalyAlert(
                id=str(uuid.uuid4())[:8],
                timestamp=reading.timestamp,
                device_id=reading.device_id,
                severity=severity,
                anomaly_flags=reading.anomaly_flags,
                anomaly_types=reading.anomaly_types,
                parameter_values={
                    "ph": reading.ph,
                    "turbidity": reading.turbidity,
                    "do_mg_l": reading.do_mg_l,
                    "temp_c": reading.temp_c
                },
                ekf_nis=reading.ekf_nis,
                message=f"Contamination Alert on Node {reading.device_id}: {', '.join(reading.anomaly_types)}"
            )
            self.storage.write_alert(alert)

        # Push to live WebSocket subscribers
        await self.broadcast_websocket(reading)

    async def broadcast_websocket(self, reading: WaterQualityReading):
        """Broadcast JSON payload to all connected dashboard websockets."""
        if not self.websocket_subscribers:
            return

        payload = reading.model_dump_json()
        dead_sockets = set()

        for ws in list(self.websocket_subscribers):
            try:
                await ws.send_text(payload)
            except Exception:
                dead_sockets.add(ws)

        for dead in dead_sockets:
            self.websocket_subscribers.discard(dead)

    def register_websocket(self, ws: WebSocket):
        self.websocket_subscribers.add(ws)

    def unregister_websocket(self, ws: WebSocket):
        self.websocket_subscribers.discard(ws)

    def get_system_status(self) -> SystemStatus:
        return SystemStatus(
            status="healthy",
            uptime_seconds=round(time.time() - self.start_time, 1),
            total_packets_received=self.total_packets,
            active_websocket_clients=len(self.websocket_subscribers),
            influxdb_connected=self.storage.connected_to_influx,
            fallback_storage_active=True,
            latest_packet_time=self.latest_reading.timestamp if self.latest_reading else None,
            active_device_id=self.latest_reading.device_id if self.latest_reading else 1
        )

    async def start_mock_generator(self):
        """
        Background task to simulate real-time water quality changes,
        autonomous patrol rover navigation, and occasional industrial runoff spikes.
        """
        sim_conf = self.config.get("simulator", {})
        if not sim_conf.get("auto_generate_mock_data", True):
            return

        interval = sim_conf.get("mock_interval_seconds", 5.0)
        logger.info(f"Starting simulated telemetry generator (interval: {interval}s)...")

        t = 0.0
        while True:
            try:
                await asyncio.sleep(interval)
                t += interval

                # 1. Rover patrol position interpolation
                wp_target = self.waypoints[self.current_waypoint_idx]
                prev_lat = self.latest_reading.latitude if self.latest_reading else wp_target.latitude
                prev_lon = self.latest_reading.longitude if self.latest_reading else wp_target.longitude

                # Move towards current waypoint
                d_lat = wp_target.latitude - prev_lat
                d_lon = wp_target.longitude - prev_lon
                dist = math.sqrt(d_lat**2 + d_lon**2)

                if dist < 0.0003: # Arrived at waypoint
                    wp_target.target_reached = True
                    self.current_waypoint_idx = (self.current_waypoint_idx + 1) % len(self.waypoints)
                    new_lat = wp_target.latitude
                    new_lon = wp_target.longitude
                else:
                    step = 0.00015
                    new_lat = prev_lat + (d_lat / dist) * step
                    new_lon = prev_lon + (d_lon / dist) * step

                # 2. Water quality natural wave variation
                base_ph = 7.4 + 0.3 * math.sin(t * 0.05)
                base_turb = 8.0 + 4.0 * math.sin(t * 0.03 + 1.0)
                base_do = 8.2 + 0.8 * math.cos(t * 0.04)
                base_temp = 19.5 + 1.5 * math.sin(t * 0.02)

                # Solar day/night cycle simulation
                solar_val = max(0, int(75.0 + 20.0 * math.sin(t * 0.01)))
                batt_mv = 3980 - int(20 * math.cos(t * 0.01))

                flags = 0
                nis = 1.2 + 0.3 * math.sin(t * 0.1)

                # Simulate periodic anomaly event (e.g. industrial runoff around agricultural point)
                if 120 < (t % 300) < 150:
                    base_ph = 5.4      # Acidic spike
                    base_turb = 62.0   # Turbid runoff
                    base_do = 3.2      # Low DO
                    nis = 16.5         # High NIS
                    flags |= (FLAG_PH | FLAG_TURB | FLAG_DO | FLAG_NIS)

                mock_reading = WaterQualityReading(
                    timestamp=int(time.time()),
                    device_id=1,
                    latitude=new_lat,
                    longitude=new_lon,
                    ph=round(base_ph, 2),
                    turbidity=round(base_turb, 1),
                    do_mg_l=round(base_do, 2),
                    temp_c=round(base_temp, 2),
                    battery_mv=batt_mv,
                    battery_pct=round(estimate_battery_soc(batt_mv), 1),
                    solar_power_pct=solar_val,
                    anomaly_flags=flags,
                    ekf_nis=round(nis, 2),
                    is_anomaly=(flags != 0),
                    anomaly_types=["Abnormal pH", "High Turbidity", "Low Dissolved Oxygen"] if flags else []
                )

                await self.ingest_reading(mock_reading)
            except asyncio.CancelledError:
                break
            except Exception as e:
                logger.error(f"Error in simulated telemetry loop: {e}")

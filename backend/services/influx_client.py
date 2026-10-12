"""
InfluxDB 2.x client and local SQLite persistence engine.
Provides transparent fallback so the system operates seamlessly whether
InfluxDB is actively running or offline in standalone developer mode.
"""

import os
import sqlite3
import time
import logging
from typing import List, Optional, Dict, Any
from backend.api.models import WaterQualityReading, AnomalyAlert

logger = logging.getLogger("waterquality.storage")

class InfluxStorageClient:
    def __init__(self, config: Dict[str, Any]):
        self.config = config
        self.db_config = config.get("database", {})
        self.influx_url = self.db_config.get("influxdb_url", "http://influxdb:8086")
        self.token = self.db_config.get("token", "")
        self.org = self.db_config.get("org", "water_quality_org")
        self.bucket = self.db_config.get("bucket", "water_quality_data")
        self.fallback_active = self.db_config.get("enable_fallback_storage", True)
        self.fallback_path = self.db_config.get("fallback_db_path", "data/telemetry_fallback.db")

        self.client = None
        self.write_api = None
        self.query_api = None
        self.connected_to_influx = False

        self._init_sqlite_fallback()
        self._try_connect_influx()

    def _init_sqlite_fallback(self):
        """Initialise SQLite database schema for zero-dependency local storage."""
        try:
            os.makedirs(os.path.dirname(self.fallback_path), exist_ok=True)
        except Exception:
            pass

        try:
            conn = sqlite3.connect(self.fallback_path)
            cursor = conn.cursor()
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS telemetry (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    timestamp INTEGER NOT NULL,
                    device_id INTEGER NOT NULL,
                    latitude REAL,
                    longitude REAL,
                    ph REAL,
                    turbidity REAL,
                    do_mg_l REAL,
                    temp_c REAL,
                    battery_mv INTEGER,
                    battery_pct REAL,
                    solar_power_pct INTEGER,
                    anomaly_flags INTEGER,
                    ekf_nis REAL,
                    is_anomaly INTEGER
                )
            """)
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS alerts (
                    id TEXT PRIMARY KEY,
                    timestamp INTEGER NOT NULL,
                    device_id INTEGER NOT NULL,
                    severity TEXT NOT NULL,
                    anomaly_flags INTEGER,
                    anomaly_types TEXT,
                    message TEXT,
                    ekf_nis REAL
                )
            """)
            cursor.execute("CREATE INDEX IF NOT EXISTS idx_telemetry_ts ON telemetry(timestamp)")
            cursor.execute("CREATE INDEX IF NOT EXISTS idx_alerts_ts ON alerts(timestamp)")
            conn.commit()
            conn.close()
            logger.info(f"SQLite fallback database verified at {self.fallback_path}")
        except Exception as e:
            logger.warning(f"Could not initialize SQLite fallback database: {e}")

    def _try_connect_influx(self):
        """Attempt connection to InfluxDB 2.x instance."""
        try:
            from influxdb_client import InfluxDBClient
            from influxdb_client.client.write_api import SYNCHRONOUS
            self.client = InfluxDBClient(url=self.influx_url, token=self.token, org=self.org, timeout=3000)
            health = self.client.health()
            if health.status == "pass":
                self.write_api = self.client.write_api(write_options=SYNCHRONOUS)
                self.query_api = self.client.query_api()
                self.connected_to_influx = True
                logger.info(f"Connected successfully to InfluxDB at {self.influx_url}")
            else:
                logger.info(f"InfluxDB reachable but health check returned: {health.status}. Using fallback storage.")
                self.connected_to_influx = False
        except Exception as e:
            logger.info(f"InfluxDB not reachable ({e}). Standalone SQLite fallback active.")
            self.connected_to_influx = False

    def write_reading(self, reading: WaterQualityReading):
        """Write a reading to InfluxDB and/or SQLite fallback store."""
        # 1. Store in SQLite fallback
        try:
            conn = sqlite3.connect(self.fallback_path)
            cursor = conn.cursor()
            cursor.execute("""
                INSERT INTO telemetry (
                    timestamp, device_id, latitude, longitude,
                    ph, turbidity, do_mg_l, temp_c,
                    battery_mv, battery_pct, solar_power_pct,
                    anomaly_flags, ekf_nis, is_anomaly
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """, (
                reading.timestamp, reading.device_id, reading.latitude, reading.longitude,
                reading.ph, reading.turbidity, reading.do_mg_l, reading.temp_c,
                reading.battery_mv, reading.battery_pct, reading.solar_power_pct,
                reading.anomaly_flags, reading.ekf_nis, 1 if reading.is_anomaly else 0
            ))
            conn.commit()
            conn.close()
        except Exception as e:
            logger.error(f"Error persisting to SQLite: {e}")

        # 2. Store in InfluxDB if available
        if self.connected_to_influx and self.write_api:
            try:
                from influxdb_client import Point
                point = (
                    Point("water_quality")
                    .tag("device_id", str(reading.device_id))
                    .field("ph", float(reading.ph))
                    .field("turbidity", float(reading.turbidity))
                    .field("do_mg_l", float(reading.do_mg_l))
                    .field("temp_c", float(reading.temp_c))
                    .field("battery_mv", int(reading.battery_mv))
                    .field("battery_pct", float(reading.battery_pct))
                    .field("solar_power_pct", int(reading.solar_power_pct))
                    .field("anomaly_flags", int(reading.anomaly_flags))
                    .field("ekf_nis", float(reading.ekf_nis))
                    .field("latitude", float(reading.latitude))
                    .field("longitude", float(reading.longitude))
                    .time(reading.timestamp * 1_000_000_000)
                )
                self.write_api.write(bucket=self.bucket, org=self.org, record=point)
            except Exception as e:
                logger.warning(f"Failed writing point to InfluxDB: {e}. Mark offline.")
                self.connected_to_influx = False

    def write_alert(self, alert: AnomalyAlert):
        """Write an anomaly alert event to the database."""
        try:
            conn = sqlite3.connect(self.fallback_path)
            cursor = conn.cursor()
            cursor.execute("""
                INSERT OR REPLACE INTO alerts (
                    id, timestamp, device_id, severity,
                    anomaly_flags, anomaly_types, message, ekf_nis
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            """, (
                alert.id, alert.timestamp, alert.device_id, alert.severity,
                alert.anomaly_flags, ",".join(alert.anomaly_types), alert.message, alert.ekf_nis
            ))
            conn.commit()
            conn.close()
        except Exception as e:
            logger.error(f"Error persisting alert to SQLite: {e}")

    def query_telemetry(self, from_ts: Optional[int] = None, to_ts: Optional[int] = None,
                        device_id: Optional[int] = None, limit: int = 100) -> List[WaterQualityReading]:
        """Fetch historical water quality telemetry points."""
        conn = sqlite3.connect(self.fallback_path)
        cursor = conn.cursor()

        query = "SELECT timestamp, device_id, latitude, longitude, ph, turbidity, do_mg_l, temp_c, battery_mv, battery_pct, solar_power_pct, anomaly_flags, ekf_nis, is_anomaly FROM telemetry WHERE 1=1"
        params = []

        if from_ts is not None:
            query += " AND timestamp >= ?"
            params.append(from_ts)
        if to_ts is not None:
            query += " AND timestamp <= ?"
            params.append(to_ts)
        if device_id is not None:
            query += " AND device_id = ?"
            params.append(device_id)

        query += " ORDER BY timestamp DESC LIMIT ?"
        params.append(limit)

        cursor.execute(query, params)
        rows = cursor.fetchall()
        conn.close()

        results = []
        for r in reversed(rows):
            results.append(WaterQualityReading(
                timestamp=r[0],
                device_id=r[1],
                latitude=r[2],
                longitude=r[3],
                ph=r[4],
                turbidity=r[5],
                do_mg_l=r[6],
                temp_c=r[7],
                battery_mv=r[8],
                battery_pct=r[9],
                solar_power_pct=r[10],
                anomaly_flags=r[11],
                ekf_nis=r[12],
                is_anomaly=bool(r[13])
            ))
        return results

    def query_alerts(self, limit: int = 50) -> List[AnomalyAlert]:
        """Fetch recent anomaly alerts."""
        conn = sqlite3.connect(self.fallback_path)
        cursor = conn.cursor()
        cursor.execute("SELECT id, timestamp, device_id, severity, anomaly_flags, anomaly_types, message, ekf_nis FROM alerts ORDER BY timestamp DESC LIMIT ?", (limit,))
        rows = cursor.fetchall()
        conn.close()

        alerts = []
        for r in rows:
            alerts.append(AnomalyAlert(
                id=r[0],
                timestamp=r[1],
                device_id=r[2],
                severity=r[3],
                anomaly_flags=r[4],
                anomaly_types=r[5].split(",") if r[5] else [],
                message=r[6],
                ekf_nis=r[7]
            ))
        return alerts

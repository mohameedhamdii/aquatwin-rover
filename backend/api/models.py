"""
Pydantic data models for the Water Quality Monitoring Backend API.
"""

from typing import List, Optional, Dict
from pydantic import BaseModel, Field
import time

class WaterQualityReading(BaseModel):
    timestamp: int = Field(default_factory=lambda: int(time.time()), description="Unix timestamp (seconds)")
    device_id: int = Field(default=1, description="Unique device ID")
    latitude: float = Field(default=51.5074, description="Latitude (degrees)")
    longitude: float = Field(default=-0.1278, description="Longitude (degrees)")
    ph: float = Field(..., description="pH value (0.0 - 14.0)")
    turbidity: float = Field(..., description="Turbidity in NTU")
    do_mg_l: float = Field(..., description="Dissolved Oxygen (mg/L)")
    temp_c: float = Field(..., description="Water Temperature (°C)")
    battery_mv: int = Field(default=3850, description="Battery voltage (mV)")
    battery_pct: float = Field(default=85.0, description="Estimated battery State of Charge (%)")
    solar_power_pct: int = Field(default=60, description="Solar panel output % (0-100)")
    anomaly_flags: int = Field(default=0, description="Bitmask of anomaly flags")
    ekf_nis: float = Field(default=1.2, description="Normalized Innovation Squared (EKF)")
    is_anomaly: bool = Field(default=False, description="True if any anomaly triggered")
    anomaly_types: List[str] = Field(default_factory=list, description="Descriptive list of active anomalies")

class RawPacketPayload(BaseModel):
    hex_payload: str = Field(..., description="Hex-encoded 32-byte binary LoRa packet")
    rssi: Optional[int] = Field(default=-85, description="Received Signal Strength Indicator in dBm")
    snr: Optional[float] = Field(default=8.5, description="Signal-to-Noise Ratio in dB")
    gateway_id: Optional[str] = Field(default="GW-01-RAK7268", description="Identifier of receiving LoRa gateway")

class AnomalyAlert(BaseModel):
    id: str = Field(..., description="Unique alert ID")
    timestamp: int = Field(..., description="Unix timestamp of alert")
    device_id: int = Field(..., description="Device that emitted the anomaly")
    severity: str = Field(..., description="Severity level: WARNING, CRITICAL, STATISTICAL")
    anomaly_flags: int = Field(..., description="Flag bitmask")
    anomaly_types: List[str] = Field(..., description="Names of triggered parameters")
    parameter_values: Dict[str, float] = Field(default_factory=dict, description="Recorded values during alert")
    ekf_nis: float = Field(default=0.0, description="Normalized Innovation Squared value")
    message: str = Field(..., description="Human-readable notification message")

class Waypoint(BaseModel):
    id: int = Field(..., description="Waypoint index (0-15)")
    latitude: float = Field(..., description="Target Latitude (degrees)")
    longitude: float = Field(..., description="Target Longitude (degrees)")
    label: str = Field(default="", description="Descriptive label (e.g. Inflow Zone)")
    target_reached: bool = Field(default=False, description="Whether waypoint has been reached")

class SystemStatus(BaseModel):
    status: str = "healthy"
    uptime_seconds: float = 0.0
    total_packets_received: int = 0
    active_websocket_clients: int = 0
    influxdb_connected: bool = False
    fallback_storage_active: bool = True
    latest_packet_time: Optional[int] = None
    active_device_id: int = 1

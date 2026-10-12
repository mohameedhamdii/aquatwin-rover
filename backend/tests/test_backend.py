"""
Unit and integration tests for backend API, packet unpacking, and storage.
"""

import time
import pytest
from fastapi.testclient import TestClient
from backend.main import app
from backend.services.data_service import (
    DataService,
    estimate_battery_soc,
    FLAG_PH,
    FLAG_TURB,
    FLAG_DO,
    FLAG_NIS
)
from backend.api.models import WaterQualityReading

@pytest.fixture
def client():
    with TestClient(app) as c:
        yield c

def test_battery_soc_estimation():
    assert estimate_battery_soc(4200) == 100.0
    assert estimate_battery_soc(3300) == 0.0
    assert 15.0 <= estimate_battery_soc(3650) <= 25.0
    assert 65.0 <= estimate_battery_soc(3900) <= 75.0

def test_packet_pack_and_unpack(client):
    service = app.state.data_service

    orig = WaterQualityReading(
        timestamp=1700000000,
        device_id=5,
        latitude=51.5074,
        longitude=-0.1278,
        ph=7.24,
        turbidity=12.5,
        do_mg_l=8.15,
        temp_c=18.40,
        battery_mv=3850,
        battery_pct=65.0,
        solar_power_pct=80,
        anomaly_flags=FLAG_PH | FLAG_NIS,
        ekf_nis=14.2,
        is_anomaly=True,
        anomaly_types=["Abnormal pH Level", "Statistical NIS Divergence"]
    )

    packed_bytes = service.encode_binary_packet(orig)
    assert len(packed_bytes) == 32

    decoded = service.decode_binary_packet(packed_bytes)
    assert decoded.timestamp == 1700000000
    assert decoded.device_id == 5
    assert abs(decoded.latitude - 51.5074) < 1e-4
    assert abs(decoded.longitude - (-0.1278)) < 1e-4
    assert abs(decoded.ph - 7.24) < 0.02
    assert abs(decoded.turbidity - 12.5) < 0.2
    assert abs(decoded.do_mg_l - 8.15) < 0.02
    assert abs(decoded.temp_c - 18.40) < 0.02
    assert decoded.battery_mv == 3850
    assert decoded.solar_power_pct == 80
    assert decoded.anomaly_flags == (FLAG_PH | FLAG_NIS)
    assert decoded.is_anomaly is True

def test_api_status(client):
    res = client.get("/api/v1/status")
    assert res.status_code == 200
    data = res.json()
    assert data["status"] == "healthy"
    assert "uptime_seconds" in data
    assert "total_packets_received" in data

def test_api_post_json_reading(client):
    payload = {
        "timestamp": int(time.time()),
        "device_id": 2,
        "latitude": 51.5080,
        "longitude": -0.1270,
        "ph": 7.35,
        "turbidity": 5.2,
        "do_mg_l": 8.60,
        "temp_c": 19.10,
        "battery_mv": 3950,
        "battery_pct": 75.0,
        "solar_power_pct": 90,
        "anomaly_flags": 0,
        "ekf_nis": 0.85,
        "is_anomaly": False,
        "anomaly_types": []
    }
    res = client.post("/api/v1/data", json=payload)
    assert res.status_code == 200
    data = res.json()
    assert data["device_id"] == 2
    assert data["ph"] == 7.35

def test_api_post_raw_packet(client):
    service = app.state.data_service
    test_reading = WaterQualityReading(
        timestamp=int(time.time()),
        device_id=3,
        latitude=51.5074,
        longitude=-0.1278,
        ph=6.85,
        turbidity=18.0,
        do_mg_l=7.80,
        temp_c=20.50,
        battery_mv=3780,
        battery_pct=40.0,
        solar_power_pct=45,
        anomaly_flags=0,
        ekf_nis=1.1,
        is_anomaly=False,
        anomaly_types=[]
    )
    raw_bytes = service.encode_binary_packet(test_reading)
    hex_str = raw_bytes.hex()

    res = client.post("/api/v1/packet/raw", json={"hex_payload": hex_str})
    assert res.status_code == 200
    data = res.json()
    assert data["device_id"] == 3
    assert abs(data["ph"] - 6.85) < 0.05

def test_api_get_historical_and_latest(client):
    res = client.get("/api/v1/latest")
    assert res.status_code == 200
    latest = res.json()
    assert "ph" in latest

    res2 = client.get("/api/v1/data?limit=10")
    assert res2.status_code == 200
    pts = res2.json()
    assert isinstance(pts, list)
    assert len(pts) > 0

def test_api_waypoints(client):
    res = client.get("/api/v1/waypoints")
    assert res.status_code == 200
    wps = res.json()
    assert len(wps) >= 3

    new_wp = {
        "id": 99,
        "latitude": 51.5100,
        "longitude": -0.1200,
        "label": "Test Buoy Marker",
        "target_reached": False
    }
    res2 = client.post("/api/v1/waypoints", json=new_wp)
    assert res2.status_code == 200
    assert res2.json()["label"] == "Test Buoy Marker"

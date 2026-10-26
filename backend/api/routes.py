"""
FastAPI route handlers for telemetry ingestion, historical querying,
alerts management, and rover waypoints.
"""

from typing import List, Optional
from fastapi import APIRouter, HTTPException, Depends, Query, Request
from backend.api.models import (
    WaterQualityReading,
    RawPacketPayload,
    AnomalyAlert,
    Waypoint,
    SystemStatus
)
from backend.services.data_service import DataService

router = APIRouter(prefix="/api/v1", tags=["Water Quality Telemetry"])

def get_data_service(request: Request) -> DataService:
    return request.app.state.data_service

@router.post("/data", response_model=WaterQualityReading, summary="Ingest JSON telemetry reading")
async def ingest_json_reading(reading: WaterQualityReading, service: DataService = Depends(get_data_service)):
    """Ingest already decoded JSON telemetry from HTTP gateways or simulations."""
    await service.ingest_reading(reading)
    return reading

@router.post("/packet/raw", response_model=WaterQualityReading, summary="Ingest raw binary LoRa packet")
async def ingest_raw_packet(payload: RawPacketPayload, service: DataService = Depends(get_data_service)):
    """
    Ingest a 32-byte hex-encoded binary packet transmitted by the LoRa transceiver.
    Matches the exact C `wq_packet_t` struct layout.
    """
    try:
        raw_bytes = bytes.fromhex(payload.hex_payload.strip())
        reading = service.decode_binary_packet(raw_bytes)
        if payload.rssi is not None:
            reading.rssi = payload.rssi
        await service.ingest_reading(reading)
        return reading
    except ValueError as e:
        raise HTTPException(status_code=400, detail=str(e))
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to process packet: {str(e)}")

@router.get("/latest", response_model=WaterQualityReading, summary="Get latest telemetry reading")
async def get_latest_reading(service: DataService = Depends(get_data_service)):
    """Retrieve the most recent water quality state measurement."""
    if not service.latest_reading:
        raise HTTPException(status_code=404, detail="No telemetry available yet")
    return service.latest_reading

@router.get("/data", response_model=List[WaterQualityReading], summary="Query historical telemetry time-series")
async def get_historical_data(
    from_ts: Optional[int] = Query(None, alias="from", description="Start timestamp (epoch seconds)"),
    to_ts: Optional[int] = Query(None, alias="to", description="End timestamp (epoch seconds)"),
    device_id: Optional[int] = Query(None, description="Filter by device ID"),
    limit: int = Query(100, ge=1, le=1000, description="Max points to return"),
    service: DataService = Depends(get_data_service)
):
    """Query time-series points stored in InfluxDB/SQLite for the digital-twin dashboard charts."""
    return service.storage.query_telemetry(from_ts=from_ts, to_ts=to_ts, device_id=device_id, limit=limit)

@router.get("/alerts", response_model=List[AnomalyAlert], summary="Get anomaly contamination alerts")
async def get_alerts(limit: int = Query(50, ge=1, le=200), service: DataService = Depends(get_data_service)):
    """Retrieve contamination anomaly events detected by the on-device EKF & threshold monitor."""
    return service.storage.query_alerts(limit=limit)

@router.get("/status", response_model=SystemStatus, summary="Get backend service health status")
async def get_status(service: DataService = Depends(get_data_service)):
    """Get service uptime, active clients, and connection health."""
    return service.get_system_status()

@router.get("/waypoints", response_model=List[Waypoint], summary="List rover patrol waypoints")
async def get_waypoints(service: DataService = Depends(get_data_service)):
    """Get current GPS patrol waypoints for autonomous rover navigation."""
    return service.waypoints

@router.post("/waypoints", response_model=Waypoint, summary="Add a rover patrol waypoint")
async def add_waypoint(wp: Waypoint, service: DataService = Depends(get_data_service)):
    """Add a new GPS target coordinate to the rover mission queue."""
    service.waypoints.append(wp)
    return wp

@router.post("/simulate/anomaly", summary="Trigger a simulated anomaly for live demo")
async def trigger_mock_anomaly(service: DataService = Depends(get_data_service)):
    """Injects a sudden chemical contamination event to test live dashboard warnings and alerts."""
    import time
    from backend.services.data_service import FLAG_PH, FLAG_TURB, FLAG_DO, FLAG_NIS
    shock = WaterQualityReading(
        timestamp=int(time.time()),
        device_id=1,
        latitude=service.latest_reading.latitude if service.latest_reading else 51.5074,
        longitude=service.latest_reading.longitude if service.latest_reading else -0.1278,
        ph=4.82,
        turbidity=84.5,
        do_mg_l=2.10,
        temp_c=28.4,
        battery_mv=3820,
        battery_pct=65.0,
        solar_power_pct=50,
        anomaly_flags=FLAG_PH | FLAG_TURB | FLAG_DO | FLAG_NIS,
        ekf_nis=18.4,
        is_anomaly=True,
        anomaly_types=["Severe Acidic Spike (pH 4.8)", "Industrial Effluent Turbidity (84.5 NTU)", "Severe Hypoxia (2.10 mg/L)"]
    )
    await service.ingest_reading(shock)
    return {"status": "anomaly_triggered", "reading": shock}

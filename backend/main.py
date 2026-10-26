"""
Main entry point for the Water Quality Monitoring Backend & WebSocket Server.
"""

import os
import json
import yaml
import logging
import asyncio
from contextlib import asynccontextmanager
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import RedirectResponse, FileResponse

from backend.services.influx_client import InfluxStorageClient
from backend.services.data_service import DataService
from backend.api.routes import router as api_router

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s"
)
logger = logging.getLogger("waterquality.main")

CONFIG_PATH = os.path.join(os.path.dirname(__file__), "config.yaml")

def load_config() -> dict:
    if os.path.exists(CONFIG_PATH):
        with open(CONFIG_PATH, "r") as f:
            return yaml.safe_load(f)
    return {}

config = load_config()

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    logger.info("Starting Water Quality Telemetry Service...")
    storage = InfluxStorageClient(config)
    data_service = DataService(config, storage)
    app.state.storage = storage
    app.state.data_service = data_service

    # Start simulated telemetry task in background
    sim_task = asyncio.create_task(data_service.start_mock_generator())

    yield

    # Shutdown
    logger.info("Shutting down Water Quality Telemetry Service...")
    sim_task.cancel()
    try:
        await sim_task
    except asyncio.CancelledError:
        pass

app = FastAPI(
    title="Autonomous Water Quality Monitoring Rover/Buoy API",
    description="Live telemetry ingestion, InfluxDB time-series storage, EKF anomaly detection, and WebSocket streaming.",
    version="1.0.0",
    lifespan=lifespan
)

# CORS configuration
app.add_middleware(
    CORSMiddleware,
    allow_origins=config.get("server", {}).get("cors_origins", ["*"]),
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Include API router
app.include_router(api_router)

# WebSocket live telemetry streaming
@app.websocket("/ws/live")
async def websocket_live_endpoint(websocket: WebSocket):
    await websocket.accept()
    data_service: DataService = app.state.data_service
    data_service.register_websocket(websocket)
    logger.info("New dashboard WebSocket client connected")

    try:
        # Send latest reading immediately on connection
        if data_service.latest_reading:
            await websocket.send_text(data_service.latest_reading.model_dump_json())
        else:
            await websocket.send_text(json.dumps({
                "type": "awaiting_telemetry",
                "message": "Gateway active. Awaiting initial LoRa packet transmission from field hardware node..."
            }))

        # Keep alive and listen for client messages / commands
        while True:
            msg = await websocket.receive_text()
            # If client requests forced refresh or sends waypoint update
            if msg == "ping":
                await websocket.send_text("pong")
    except WebSocketDisconnect:
        logger.info("Dashboard WebSocket client disconnected")
    except Exception as e:
        logger.warning(f"WebSocket client error: {e}")
    finally:
        data_service.unregister_websocket(websocket)

# Mount dashboard static directory if it exists
dashboard_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "dashboard"))
if os.path.exists(dashboard_dir):
    app.mount("/dashboard", StaticFiles(directory=dashboard_dir, html=True), name="dashboard")

@app.get("/", include_in_schema=False)
async def root():
    """Redirect root to dashboard if available, otherwise to docs."""
    if os.path.exists(os.path.join(dashboard_dir, "index.html")):
        return RedirectResponse(url="/dashboard/")
    return RedirectResponse(url="/docs")

if __name__ == "__main__":
    import uvicorn
    server_conf = config.get("server", {})
    uvicorn.run(
        "backend.main:app",
        host=server_conf.get("host", "0.0.0.0"),
        port=server_conf.get("port", 8000),
        reload=server_conf.get("reload", False)
    )

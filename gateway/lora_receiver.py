"""
LoRa Gateway Receiver & Forwarder.
Reads raw LoRa telemetry packets from hardware transceiver (via UART/Serial)
and forwards them to the Water Quality backend server via HTTP.
"""

import sys
import os
import time
import argparse
import logging
import struct
import requests
import yaml

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] [GATEWAY] %(message)s"
)
logger = logging.getLogger("gateway")

PACKET_SIZE = 32

def load_config(config_path="config.yaml"):
    if os.path.exists(config_path):
        with open(config_path, "r") as f:
            return yaml.safe_load(f)
    return {}

def forward_packet(backend_url: str, hex_payload: str, rssi: int = -80, snr: float = 9.0, gateway_id: str = "GW-01") -> bool:
    """Send hex-encoded 32-byte packet to backend API."""
    payload = {
        "hex_payload": hex_payload,
        "rssi": rssi,
        "snr": snr,
        "gateway_id": gateway_id
    }
    try:
        response = requests.post(backend_url, json=payload, timeout=5)
        if response.status_code == 200:
            data = response.json()
            logger.info(
                f"[FORWARD OK] Node {data.get('device_id')} | "
                f"pH: {data.get('ph')} | Turb: {data.get('turbidity')} NTU | "
                f"DO: {data.get('do_mg_l')} mg/L | Temp: {data.get('temp_c')} °C | "
                f"RSSI: {rssi} dBm"
            )
            return True
        else:
            logger.warning(f"Backend rejected packet with status {response.status_code}: {response.text}")
            return False
    except requests.exceptions.RequestException as e:
        logger.error(f"Failed to reach backend at {backend_url}: {e}")
        return False

def run_serial_receiver(config: dict):
    """Listen on hardware serial port connected to LoRa receiver MCU."""
    try:
        import serial
    except ImportError:
        logger.error("pyserial is not installed. Install via: pip install pyserial")
        sys.exit(1)

    gw_conf = config.get("gateway", {})
    be_conf = config.get("backend", {})
    port = gw_conf.get("serial_port", "COM3")
    baud = gw_conf.get("baudrate", 115200)
    backend_url = be_conf.get("url", "http://localhost:8000/api/v1/packet/raw")
    gateway_id = gw_conf.get("id", "GW-WATER-01")

    logger.info(f"Opening LoRa serial connection on {port} @ {baud} baud...")

    ser = None
    while ser is None:
        try:
            ser = serial.Serial(port, baudrate=baud, timeout=2.0)
            logger.info(f"Successfully opened LoRa receiver serial port on {port} @ {baud} baud.")
        except Exception as e:
            logger.warning(f"Could not open serial port {port} ({e}). Retrying in 5s... (Ensure LoRa USB transceiver is plugged in, or pass --demo for test mode)")
            try:
                time.sleep(5.0)
            except KeyboardInterrupt:
                logger.info("Gateway receiver terminated by user.")
                return

    logger.info("Listening for incoming LoRa RF frames...")
    buffer = bytearray()

    while True:
        try:
            incoming = ser.read(ser.in_waiting or 1)
            if incoming:
                buffer.extend(incoming)

                # If buffer contains at least 32 bytes
                while len(buffer) >= PACKET_SIZE:
                    packet_bytes = bytes(buffer[:PACKET_SIZE])
                    buffer = buffer[PACKET_SIZE:]

                    hex_str = packet_bytes.hex()
                    logger.info(f"Received 32-byte LoRa frame: {hex_str[:24]}...")
                    forward_packet(backend_url, hex_str, rssi=-78, snr=8.5, gateway_id=gateway_id)

            time.sleep(0.01)
        except KeyboardInterrupt:
            logger.info("Gateway shutting down.")
            ser.close()
            break
        except Exception as e:
            logger.error(f"Error during packet read: {e}")
            time.sleep(1.0)

def run_demo_receiver(config: dict):
    """Simulated LoRa receiver that generates and forwards synthetic 32-byte frames."""
    be_conf = config.get("backend", {})
    gw_conf = config.get("gateway", {})
    backend_url = be_conf.get("url", "http://localhost:8000/api/v1/packet/raw")
    gateway_id = gw_conf.get("id", "GW-DEMO-SIM")

    logger.info("Starting Simulated LoRa Gateway Receiver (Forwarding synthetic RF frames)...")
    seq = 0

    while True:
        try:
            seq += 1
            ts = int(time.time())
            lat = int(51.5074 * 1e7)
            lon = int(-0.1278 * 1e7)
            ph = int(7.35 * 100)
            turb = int(8.5 * 10)
            dox = int(8.10 * 100)
            temp = int(19.20 * 100)
            batt_mv = 3920
            solar_pct = 75
            flags = 0
            nis = int(1.15 * 100)
            device_id = 1
            ver = 1
            reserved = b"\x00\x00\x00\x00"

            packet_bytes = struct.pack(
                "<Iii4hHBBHBB4s",
                ts, lat, lon, ph, turb, dox, temp, batt_mv, solar_pct, flags, nis, device_id, ver, reserved
            )

            forward_packet(backend_url, packet_bytes.hex(), rssi=-82, snr=9.2, gateway_id=gateway_id)
            time.sleep(10.0)
        except KeyboardInterrupt:
            logger.info("Exiting simulated receiver.")
            break

def main():
    parser = argparse.ArgumentParser(description="Water Quality LoRa Gateway Forwarder")
    parser.add_argument("--config", default="config.yaml", help="Path to config file")
    parser.add_argument("--demo", action="store_true", help="Run in mock/demo mode without physical LoRa hardware")
    args = parser.parse_args()

    conf_path = os.path.join(os.path.dirname(__file__), args.config)
    config = load_config(conf_path)

    if args.demo:
        run_demo_receiver(config)
    else:
        run_serial_receiver(config)

if __name__ == "__main__":
    main()

# AquaTwin Aquatic Field Deployment & Operations Guide

This guide describes the complete procedure for taking the AquaTwin autonomous rover/buoy from the lab bench into field operation in a lake, river, reservoir, or retention basin.

---

## 1. Phase 1 — Bench & Laboratory Validation ("The Bucket Test")

Before deploying in natural bodies of water, perform the mandatory 3-stage bucket test:

1. **Clean Water Baseline:**
   - Fill a 20-liter clean plastic container with tap or distilled water.
   - Submerge sensor probes.
   - Verify readings over debug serial console:
     - pH: ~7.0–7.4
     - Turbidity: < 10 NTU
     - Dissolved Oxygen: 7.5–8.5 mg/L
     - Temperature: Matches ambient thermometer ($\pm 0.5\text{ °C}$)
2. **Induced Anomaly Verification:**
   - Add 50 mL of vinegar (acetic acid) to induce a drop in pH below 5.5.
   - Verify that the on-device EKF raises `ANOMALY_FLAG_PH` within 2 seconds.
   - Verify that the 3D Digital Twin dashboard highlights the rover beacon red and triggers the Contamination Warning Banner.
   - Add 1 spoonful of fine clay silt to verify turbidity spike detection (`ANOMALY_FLAG_TURB`).
3. **Radio Link & Power Validation:**
   - Move the field unit 200 meters away from the gateway (through walls) to confirm RF link margin (RSSI > -105 dBm).
   - Expose the solar panel to sunlight or a 500W halogen work lamp to verify charge current to the LiPo battery (> 300 mA).

---

## 2. Phase 2 — Mechanical Assembly & Ballasting

1. **Hull Ballasting & Waterline:**
   - Place electronics, battery, and ballast weights into the catamaran hulls.
   - Float the assembled craft in a calm test tub.
   - The waterline must sit exactly at the designated design mark (sensor probe membranes submerged at least 10 cm below surface, while the solar panel sits 12 cm above the splash zone).
2. **Cable Gland Tightening:**
   - Ensure all PG7 and PG9 cable glands are hand-tightened with a wrench.
   - Apply marine sealant (3M Marine 5200 or silicone) around cable gland threads.
3. **Electronics Box Desiccant:**
   - Insert fresh silica gel desiccant packs inside the junction box before fastening the lid screws diagonally in an X-pattern.

---

## 3. Phase 3 — Backend Server & Dashboard Deployment

### Production Deployment via Docker Compose

Run on the server or base station host:

```bash
# 1. Clone repository
git clone https://github.com/your-username/aquatwin-water-quality.git
cd aquatwin-water-quality

# 2. Launch InfluxDB and FastAPI backend
docker compose up -d

# 3. Verify health status
curl http://localhost:8000/api/v1/status
```

The system will start:
- **FastAPI Telemetry Ingestion Engine:** `http://localhost:8000`
- **3D Digital Twin Three.js Dashboard:** `http://localhost:8000/dashboard/`
- **InfluxDB 2.x Time-Series Storage:** `http://localhost:8086`

### Zero-Dependency Standalone Mode (No Docker)

If deploying on a lightweight field laptop without Docker:

```bash
pip install -r backend/requirements.txt
python backend/main.py
```
The backend automatically falls back to an embedded SQLite database engine with zero external dependencies.

---

## 4. Phase 4 — Field Launch & Mission Operation

### Buoy Mode Deployment
1. Attach stainless steel anchor chain or braided nylon tether to the central mooring eyelet on the bottom plate.
2. Deploy anchor weight (5–10 kg mushroom anchor for mud bottoms, or concrete block).
3. Power on MCU via external waterproof magnetic reed switch.
4. Verify initial telemetry burst is received on the gateway.

### Rover Mode (Patrol Navigation)
1. Using the dashboard API, submit patrol coordinates:
   ```bash
   curl -X POST http://localhost:8000/api/v1/waypoints \
        -H "Content-Type: application/json" \
        -d '{"id": 1, "latitude": 51.5085, "longitude": -0.1265, "label": "Agricultural Runoff Outfall"}'
   ```
2. Place rover in water; thrusters will engage differential steering towards WP-1.
3. **Failsafe Return-to-Home:** If the LiPo battery drops below 20%, thruster power is automatically cut, and the rover enters low-power drifting mode to preserve critical telemetry beacons.

---

## 5. Maintenance & Anti-Fouling Schedule

- **Every 14 Days:** Rinse pH and optical turbidity probe lenses with warm water and isopropyl alcohol to remove biological biofilm.
- **Every 60 Days:** Recalibrate pH probe using pH 4.0 and pH 7.0 standard buffer solutions.
- **Every 90 Days:** Inspect LiPo battery health and replace internal desiccant bags.

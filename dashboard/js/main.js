/**
 * @file main.js
 * @brief 3D Digital Twin Engine with Three.js — Water Shader, Shoreline, Rover Model & Real-Time Sync
 */

class DigitalTwinApp {
    constructor() {
        this.container = document.getElementById('app-container');
        this.canvas = document.getElementById('webgl-canvas');
        this.scene = null;
        this.camera = null;
        this.renderer = null;
        this.controls = null;
        this.water = null;
        this.rover = null;
        this.roverBeacon = null;
        this.roverLight = null;
        this.clock = new THREE.Clock();

        this.cameraMode = 'overview'; // 'overview', 'follow', 'surface'
        this.roverTargetPos = new THREE.Vector3(0, 0, 0);
        this.roverCurrentPos = new THREE.Vector3(0, 0, 0);
        this.roverHeading = 0;

        this.overlayManager = null;
        this.wsClient = null;

        this.initThree();
        this.buildEnvironment();
        this.buildRoverModel();
        this.setupCameraControls();
        this.initTelemetry();
        this.animate();
    }

    initThree() {
        // Scene setup
        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(0x07111e);
        this.scene.fog = new THREE.FogExp2(0x07111e, 0.012);

        // Camera
        this.camera = new THREE.PerspectiveCamera(
            50,
            window.innerWidth / window.innerHeight,
            0.1,
            1000
        );
        this.camera.position.set(0, 36, 52);

        // Renderer
        this.renderer = new THREE.WebGLRenderer({
            canvas: this.canvas,
            antialias: true,
            powerPreference: 'high-performance'
        });
        this.renderer.setSize(window.innerWidth, window.innerHeight);
        this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
        this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
        this.renderer.toneMappingExposure = 1.1;
        this.renderer.shadowMap.enabled = true;
        this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;

        // OrbitControls
        this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
        this.controls.enableDamping = true;
        this.controls.dampingFactor = 0.05;
        this.controls.maxPolarAngle = Math.PI / 2 - 0.05; // Prevent camera sinking under water
        this.controls.minDistance = 6;
        this.controls.maxDistance = 140;
        this.controls.target.set(0, 0, 0);

        // Lighting
        const ambientLight = new THREE.AmbientLight(0x4070a0, 0.9);
        this.scene.add(ambientLight);

        const sunLight = new THREE.DirectionalLight(0xfff5e6, 1.8);
        sunLight.position.set(40, 70, 30);
        sunLight.castShadow = true;
        sunLight.shadow.mapSize.width = 2048;
        sunLight.shadow.mapSize.height = 2048;
        sunLight.shadow.camera.near = 10;
        sunLight.shadow.camera.far = 160;
        sunLight.shadow.camera.left = -50;
        sunLight.shadow.camera.right = 50;
        sunLight.shadow.camera.top = 50;
        sunLight.shadow.camera.bottom = -50;
        this.scene.add(sunLight);

        // Sky hemisphere fill
        const hemiLight = new THREE.HemisphereLight(0x73c2fb, 0x071424, 0.6);
        this.scene.add(hemiLight);

        window.addEventListener('resize', () => this.onWindowResize());
    }

    buildEnvironment() {
        // Water surface plane with wave displacement
        this.water = new window.WaterSurface(this.scene, 200, 100);

        // Surrounding Shoreline / Lake Basin Terrain
        const terrainGeo = new THREE.RingGeometry(48, 120, 64, 8);
        terrainGeo.rotateX(-Math.PI / 2);

        // Procedural terrain elevation
        const pos = terrainGeo.attributes.position;
        for (let i = 0; i < pos.count; i++) {
            const x = pos.getX(i);
            const z = pos.getZ(i);
            const dist = Math.sqrt(x * x + z * z);
            if (dist > 48) {
                const elevation = Math.pow((dist - 48) * 0.18, 1.3) + Math.sin(x * 0.1) * Math.cos(z * 0.1) * 2.5;
                pos.setY(i, Math.max(0.2, elevation));
            }
        }
        terrainGeo.computeVertexNormals();

        const terrainMat = new THREE.MeshStandardMaterial({
            color: 0x182a20,
            roughness: 0.85,
            metalness: 0.1,
            flatShading: true
        });

        const terrainMesh = new THREE.Mesh(terrainGeo, terrainMat);
        terrainMesh.receiveShadow = true;
        this.scene.add(terrainMesh);

        // Add low-poly shoreline pine trees and rocky outcroppings
        this.buildShorelineAssets();
    }

    buildShorelineAssets() {
        const treeGroup = new THREE.Group();
        const trunkGeo = new THREE.CylinderGeometry(0.2, 0.35, 1.8, 5);
        const trunkMat = new THREE.MeshStandardMaterial({ color: 0x3d2314, roughness: 0.9 });
        const foliageGeo = new THREE.ConeGeometry(1.6, 4.0, 5);
        const foliageMat = new THREE.MeshStandardMaterial({ color: 0x1e4620, roughness: 0.8, flatShading: true });

        for (let i = 0; i < 45; i++) {
            const angle = (i / 45) * Math.PI * 2 + Math.random() * 0.1;
            const radius = 55 + Math.random() * 25;
            const x = Math.cos(angle) * radius;
            const z = Math.sin(angle) * radius;
            const y = Math.pow((radius - 48) * 0.18, 1.3) + 1.0;

            const tree = new THREE.Group();
            const trunk = new THREE.Mesh(trunkGeo, trunkMat);
            trunk.position.y = 0.9;
            trunk.castShadow = true;
            const foliage = new THREE.Mesh(foliageGeo, foliageMat);
            foliage.position.y = 3.2;
            foliage.castShadow = true;

            tree.add(trunk);
            tree.add(foliage);
            tree.position.set(x, y, z);
            const scale = 0.8 + Math.random() * 0.6;
            tree.scale.set(scale, scale, scale);
            treeGroup.add(tree);
        }
        this.scene.add(treeGroup);
    }

    buildRoverModel() {
        // High-detail procedural autonomous catamaran rover
        this.rover = new THREE.Group();

        // 1. Twin Catamaran Pontoon Hulls
        const hullGeo = new THREE.BoxGeometry(0.8, 0.7, 4.2);
        const hullMat = new THREE.MeshStandardMaterial({
            color: 0x00d2ff,
            metalness: 0.4,
            roughness: 0.3
        });

        const leftHull = new THREE.Mesh(hullGeo, hullMat);
        leftHull.position.set(-1.2, 0.1, 0);
        leftHull.castShadow = true;

        const rightHull = new THREE.Mesh(hullGeo, hullMat);
        rightHull.position.set(1.2, 0.1, 0);
        rightHull.castShadow = true;

        this.rover.add(leftHull);
        this.rover.add(rightHull);

        // 2. Central Equipment Deck & IP67 Junction Box
        const deckGeo = new THREE.BoxGeometry(2.4, 0.25, 3.2);
        const deckMat = new THREE.MeshStandardMaterial({ color: 0x1e293b, roughness: 0.5 });
        const deck = new THREE.Mesh(deckGeo, deckMat);
        deck.position.set(0, 0.45, 0);
        deck.castShadow = true;
        this.rover.add(deck);

        const enclosureGeo = new THREE.BoxGeometry(1.6, 0.6, 2.0);
        const enclosureMat = new THREE.MeshStandardMaterial({ color: 0xe2e8f0, roughness: 0.2 });
        const enclosure = new THREE.Mesh(enclosureGeo, enclosureMat);
        enclosure.position.set(0, 0.85, 0.1);
        enclosure.castShadow = true;
        this.rover.add(enclosure);

        // 3. Solar Panel Photovoltaic Top
        const solarGeo = new THREE.BoxGeometry(1.5, 0.08, 1.9);
        const solarMat = new THREE.MeshStandardMaterial({
            color: 0x021b3a,
            roughness: 0.15,
            metalness: 0.85
        });
        const solarPanel = new THREE.Mesh(solarGeo, solarMat);
        solarPanel.position.set(0, 1.2, 0.1);
        this.rover.add(solarPanel);

        // 4. Sensor Mast & LoRa Antenna
        const mastGeo = new THREE.CylinderGeometry(0.06, 0.06, 2.2, 8);
        const mastMat = new THREE.MeshStandardMaterial({ color: 0x475569, metalness: 0.8 });
        const mast = new THREE.Mesh(mastGeo, mastMat);
        mast.position.set(0, 1.9, -0.6);
        this.rover.add(mast);

        const antennaGeo = new THREE.CylinderGeometry(0.02, 0.02, 1.4, 6);
        const antenna = new THREE.Mesh(antennaGeo, mastMat);
        antenna.position.set(0.4, 1.8, -0.6);
        this.rover.add(antenna);

        // 5. Status Beacon Light on Mast Top
        const beaconGeo = new THREE.SphereGeometry(0.18, 16, 16);
        const beaconMat = new THREE.MeshBasicMaterial({ color: 0x10b981 });
        this.roverBeacon = new THREE.Mesh(beaconGeo, beaconMat);
        this.roverBeacon.position.set(0, 3.0, -0.6);
        this.rover.add(this.roverBeacon);

        this.roverLight = new THREE.PointLight(0x10b981, 2.5, 12);
        this.roverLight.position.set(0, 3.0, -0.6);
        this.rover.add(this.roverLight);

        // 6. Submerged Water Quality Sensor Probes (pH, Turbidity, DO, Temp)
        const probeGroup = new THREE.Group();
        const probeGeo = new THREE.CylinderGeometry(0.06, 0.06, 0.9, 8);
        const probeMat = new THREE.MeshStandardMaterial({ color: 0x0f172a });

        for (let i = -0.3; i <= 0.3; i += 0.2) {
            const probe = new THREE.Mesh(probeGeo, probeMat);
            probe.position.set(i, -0.4, 0.5);
            probeGroup.add(probe);
        }
        this.rover.add(probeGroup);

        // Add rover to scene
        this.scene.add(this.rover);
    }

    setupCameraControls() {
        const btnOverview = document.getElementById('cam-overview');
        const btnFollow = document.getElementById('cam-follow');
        const btnSurface = document.getElementById('cam-surface');

        const setCam = (mode, btn) => {
            this.cameraMode = mode;
            [btnOverview, btnFollow, btnSurface].forEach(b => b.classList.remove('active'));
            btn.classList.add('active');

            if (mode === 'overview') {
                this.controls.target.set(0, 0, 0);
                this.camera.position.set(0, 36, 52);
            }
        };

        if (btnOverview) btnOverview.addEventListener('click', () => setCam('overview', btnOverview));
        if (btnFollow) btnFollow.addEventListener('click', () => setCam('follow', btnFollow));
        if (btnSurface) btnSurface.addEventListener('click', () => setCam('surface', btnSurface));
    }

    initTelemetry() {
        this.overlayManager = new window.DataOverlayManager();

        this.wsClient = new window.TelemetryWebSocket(
            (reading) => this.onTelemetryReceived(reading),
            (isOnline, statusText) => {
                const dot = document.getElementById('ws-indicator-dot');
                const text = document.getElementById('ws-status-text');
                if (dot) dot.className = `ws-indicator ${isOnline ? 'live' : 'offline'}`;
                if (text) text.textContent = statusText;
            }
        );

        // Listen for mock spill demo event
        window.addEventListener('aqua:mockSpill', () => {
            const mockSpillReading = {
                timestamp: Math.floor(Date.now() / 1000),
                device_id: 1,
                latitude: this.roverCurrentPos.x * 0.0001 + 51.5074,
                longitude: this.roverCurrentPos.z * 0.0001 - 0.1278,
                ph: 4.85,
                turbidity: 78.4,
                do_mg_l: 2.30,
                temp_c: 27.8,
                battery_mv: 3820,
                battery_pct: 65.0,
                solar_power_pct: 55,
                anomaly_flags: 0x1F,
                ekf_nis: 17.8,
                is_anomaly: true,
                anomaly_types: ["Severe Acidic Spike (pH 4.85)", "Industrial Effluent Plume (78.4 NTU)", "Severe Hypoxia (2.30 mg/L)"]
            };
            this.onTelemetryReceived(mockSpillReading);
        });
    }

    onTelemetryReceived(reading) {
        // 1. Update HUD components & charts
        this.overlayManager.updateTelemetryUI(reading);

        // 2. Map GPS coordinates to 3D world space coordinates
        // Reference origin: (51.5074, -0.1278) -> (0, 0) in 3D
        const dLat = (reading.latitude - 51.5074) * 12000.0;
        const dLon = (reading.longitude - (-0.1278)) * 12000.0;

        // Constrain rover inside lake boundaries
        this.roverTargetPos.x = Math.max(-36, Math.min(36, dLon));
        this.roverTargetPos.z = Math.max(-36, Math.min(36, -dLat));

        // 3. Update status beacon color based on anomaly state
        if (this.roverBeacon && this.roverLight) {
            if (reading.is_anomaly) {
                this.roverBeacon.material.color.setHex(0xf43f5e); // Crimson red
                this.roverLight.color.setHex(0xf43f5e);
            } else if (reading.ph < 6.5 || reading.ph > 8.5 || reading.turbidity > 25.0) {
                this.roverBeacon.material.color.setHex(0xf59e0b); // Amber caution
                this.roverLight.color.setHex(0xf59e0b);
            } else {
                this.roverBeacon.material.color.setHex(0x10b981); // Emerald green normal
                this.roverLight.color.setHex(0x10b981);
            }
        }
    }

    onWindowResize() {
        this.camera.aspect = window.innerWidth / window.innerHeight;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(window.innerWidth, window.innerHeight);
    }

    animate() {
        requestAnimationFrame(() => this.animate());

        const elapsedTime = this.clock.getElapsedTime();

        // 1. Update water wave shader
        if (this.water) {
            this.water.update(elapsedTime);
        }

        // 2. Interpolate rover motion towards target coordinates
        if (this.rover) {
            this.roverCurrentPos.lerp(this.roverTargetPos, 0.035);

            // Calculate wave height & bobbing at rover's current water coordinates
            const waveY = this.water ? this.water.getWaterHeightAt(this.roverCurrentPos.x, this.roverCurrentPos.z, elapsedTime) : 0;
            this.rover.position.set(this.roverCurrentPos.x, waveY + 0.15, this.roverCurrentPos.z);

            // Gentle wave pitch & roll simulation
            this.rover.rotation.x = Math.sin(elapsedTime * 2.5) * 0.04;
            this.rover.rotation.z = Math.cos(elapsedTime * 2.0) * 0.05;

            // Turn rover heading in movement direction
            const dx = this.roverTargetPos.x - this.roverCurrentPos.x;
            const dz = this.roverTargetPos.z - this.roverCurrentPos.z;
            if (Math.sqrt(dx * dx + dz * dz) > 0.05) {
                const targetHeading = Math.atan2(dx, dz);
                this.rover.rotation.y = THREE.MathUtils.lerp(this.rover.rotation.y, targetHeading, 0.05);
            }

            // Beacon flash animation
            if (this.roverLight) {
                this.roverLight.intensity = 1.8 + Math.sin(elapsedTime * 6.0) * 0.8;
            }
        }

        // 3. Camera modes handling
        if (this.cameraMode === 'follow' && this.rover) {
            const offset = new THREE.Vector3(0, 7, 14);
            const targetCamPos = this.rover.position.clone().add(offset);
            this.camera.position.lerp(targetCamPos, 0.05);
            this.controls.target.lerp(this.rover.position, 0.08);
        } else if (this.cameraMode === 'surface' && this.rover) {
            const surfacePos = new THREE.Vector3(this.rover.position.x - 8, 2.0, this.rover.position.z + 8);
            this.camera.position.lerp(surfacePos, 0.05);
            this.controls.target.lerp(this.rover.position, 0.08);
        }

        this.controls.update();
        this.renderer.render(this.scene, this.camera);
    }
}

// Instantiate on DOM load
window.addEventListener('DOMContentLoaded', () => {
    new DigitalTwinApp();
});

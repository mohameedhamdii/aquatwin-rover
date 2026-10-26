/**
 * @file main.js
 * @brief 3D Digital Twin Engine with Three.js — Water Shader, Shoreline, Rover Model & Real-Time Sync
 */

// Shared geo/world constants for hardware telemetry coordinate mapping
const GEO_ORIGIN = { lat: 51.5074, lon: -0.1278 };
const UNITS_PER_DEGREE = 12000.0;
const LAKE_RADIUS = 38;

// One terrain height function used by both the mesh and the trees standing on it
function terrainHeight(x, z) {
    const dist = Math.hypot(x, z);
    const e = Math.pow(Math.max(0, dist - 48) * 0.18, 1.3) + Math.sin(x * 0.1) * Math.cos(z * 0.1) * 2.5;
    return Math.max(0.2, e);
}

class DigitalTwinApp {
    constructor() {
        if (typeof THREE === 'undefined') {
            console.error('[AquaTwin 3D] Three.js library not loaded.');
            return;
        }
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
        this.lastRoverPos = new THREE.Vector3();
        this.flyTo = null;
        this.elapsed = 0;

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
        this.controls.addEventListener('start', () => { this.flyTo = null; }); // user input cancels camera transitions

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
            pos.setY(i, terrainHeight(x, z));
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
            const y = terrainHeight(x, z) - 0.1; // plant the trunk base slightly into the ground

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
        this.rover.rotation.order = 'YXZ'; // heading first, so pitch/roll stay in the hull's own frame

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

            // Smooth fly-to; follow modes then ride along with the rover while staying orbitable
            const presets = {
                overview: { follow: false, offset: new THREE.Vector3(0, 36, 52) },
                follow:   { follow: true,  offset: new THREE.Vector3(0, 7, 14) },
                surface:  { follow: true,  offset: new THREE.Vector3(-8, 2, 8) }
            };
            this.flyTo = presets[mode];
        };

        if (btnOverview) btnOverview.addEventListener('click', () => setCam('overview', btnOverview));
        if (btnFollow) btnFollow.addEventListener('click', () => setCam('follow', btnFollow));
        if (btnSurface) btnSurface.addEventListener('click', () => setCam('surface', btnSurface));
    }

    initTelemetry() {
        this.overlayManager = new window.DataOverlayManager();

        this.wsClient = new window.TelemetryWebSocket(
            (reading) => this.onTelemetryReceived(reading),
            (mode, statusText) => {
                const dot = document.getElementById('ws-indicator-dot');
                const text = document.getElementById('ws-status-text');
                if (dot) dot.className = `ws-indicator ${mode}`;
                if (text) text.textContent = statusText;
            }
        );
    }

    onTelemetryReceived(reading) {
        if (!reading || reading.awaiting) {
            this.overlayManager.updateTelemetryUI(reading);
            return;
        }

        // 1. Update HUD components & charts
        this.overlayManager.updateTelemetryUI(reading);

        // 2. Map GPS coordinates to 3D world space: north = -Z, east = +X
        if (Number.isFinite(reading.latitude) && Number.isFinite(reading.longitude)) {
            const x = (reading.longitude - GEO_ORIGIN.lon) * UNITS_PER_DEGREE;
            const z = -(reading.latitude - GEO_ORIGIN.lat) * UNITS_PER_DEGREE;
            const r = Math.hypot(x, z);
            const k = r > LAKE_RADIUS ? LAKE_RADIUS / r : 1; // keep the rover inside the circular lake
            this.roverTargetPos.set(x * k, 0, z * k);
        }

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

        // Single clock read per frame (getElapsedTime + getDelta would corrupt each other)
        const dt = Math.min(this.clock.getDelta(), 0.1);
        this.elapsed += dt;
        const elapsedTime = this.elapsed;

        if (this.water) this.water.update(elapsedTime);

        if (this.rover) {
            // Frame-rate independent smoothing
            this.roverCurrentPos.lerp(this.roverTargetPos, 1 - Math.exp(-2.2 * dt));

            const waveY = this.water ? this.water.getWaterHeightAt(this.roverCurrentPos.x, this.roverCurrentPos.z, elapsedTime) : 0;
            this.rover.position.set(this.roverCurrentPos.x, waveY + 0.15, this.roverCurrentPos.z);

            this.rover.rotation.x = Math.sin(elapsedTime * 2.5) * 0.04;
            this.rover.rotation.z = Math.cos(elapsedTime * 2.0) * 0.05;

            // Turn toward travel direction via the shortest arc (no spinning the long way round)
            const dx = this.roverTargetPos.x - this.roverCurrentPos.x;
            const dz = this.roverTargetPos.z - this.roverCurrentPos.z;
            if (Math.hypot(dx, dz) > 0.05) {
                const err = Math.atan2(dx, dz) - this.rover.rotation.y;
                this.rover.rotation.y += Math.atan2(Math.sin(err), Math.cos(err)) * (1 - Math.exp(-3 * dt));
            }

            if (this.roverLight) {
                this.roverLight.intensity = 1.8 + Math.sin(elapsedTime * 6.0) * 0.8;
            }

            // Follow modes: move camera + target by the rover's motion so the user can still orbit/zoom
            if (this.cameraMode !== 'overview') {
                const delta = this.roverCurrentPos.clone().sub(this.lastRoverPos);
                this.camera.position.add(delta);
                this.controls.target.add(delta);
            }
            this.lastRoverPos.copy(this.roverCurrentPos);
        }

        if (this.flyTo) {
            const base = this.flyTo.follow ? this.roverCurrentPos : new THREE.Vector3();
            const goal = base.clone().add(this.flyTo.offset);
            this.camera.position.lerp(goal, 1 - Math.exp(-4 * dt));
            this.controls.target.lerp(base, 1 - Math.exp(-5 * dt));
            if (this.camera.position.distanceTo(goal) < 0.2) this.flyTo = null;
        }

        this.controls.update();
        this.renderer.render(this.scene, this.camera);
    }
}

// Instantiate on DOM load
window.addEventListener('DOMContentLoaded', () => {
    new DigitalTwinApp();
});

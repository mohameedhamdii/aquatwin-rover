/**
 * @file websocket.js
 * @brief Live telemetry socket with auto-reconnect and a clearly labelled local demo fallback.
 *
 * onStatus(mode, text) — mode is 'live' | 'demo' | 'offline'.
 */

class TelemetryWebSocket {
    constructor(onMessageCallback, onStatusCallback) {
        this.onMessage = onMessageCallback;
        this.onStatus = onStatusCallback;
        this.socket = null;
        this.reconnectAttempts = 0;
        this.maxReconnectDelay = 5000;
        this.pingInterval = null;
        this.reconnectTimer = null;
        this.demoTimer = null;
        this.demoTick = 0;
        this.spillTicksLeft = 0;

        this.connect();
    }

    getWsUrl() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const host = window.location.host || 'localhost:8000';
        return `${protocol}//${host}/ws/live`;
    }

    setStatus(mode, text) {
        if (this.onStatus) this.onStatus(mode, text);
    }

    connect() {
        clearTimeout(this.reconnectTimer);
        let socket;
        try {
            socket = new WebSocket(this.getWsUrl());
        } catch (err) {
            console.warn('[AquaTwin WS] Could not open socket:', err);
            this.handleDisconnect();
            return;
        }
        this.socket = socket;

        socket.onopen = () => {
            console.log('[AquaTwin WS] Connected to live backend stream.');
            this.reconnectAttempts = 0;
            this.stopDemo();
            this.setStatus('live', 'LIVE STREAM');

            clearInterval(this.pingInterval);
            this.pingInterval = setInterval(() => {
                if (socket.readyState === WebSocket.OPEN) socket.send('ping');
            }, 15000);
        };

        socket.onmessage = (event) => {
            if (event.data === 'pong') return;
            try {
                const reading = JSON.parse(event.data);
                if (this.onMessage) this.onMessage(reading);
            } catch (e) {
                console.error('[AquaTwin WS] Error parsing telemetry payload:', e);
            }
        };

        socket.onerror = (err) => console.warn('[AquaTwin WS] Socket error:', err);

        // Ignore close events from a socket we've already replaced
        socket.onclose = () => {
            if (this.socket === socket) this.handleDisconnect();
        };
    }

    handleDisconnect() {
        clearInterval(this.pingInterval);
        this.reconnectAttempts++;
        const delay = Math.min(1000 * Math.pow(1.5, this.reconnectAttempts), this.maxReconnectDelay);

        if (this.reconnectAttempts >= 2) {
            this.startDemo();
        } else {
            this.setStatus('offline', 'OFFLINE / RECONNECTING');
        }

        clearTimeout(this.reconnectTimer);
        this.reconnectTimer = setTimeout(() => this.connect(), delay);
    }

    // ---- Client-side demo generator (only runs while the backend is unreachable) ----

    startDemo() {
        if (this.demoTimer) return;
        console.info('[AquaTwin WS] Backend unreachable — starting simulated demo data.');
        this.setStatus('demo', 'DEMO MODE (SIMULATED)');
        this.demoStep(); // show data immediately instead of waiting one interval
        this.demoTimer = setInterval(() => this.demoStep(), 3000);
    }

    stopDemo() {
        clearInterval(this.demoTimer);
        this.demoTimer = null;
    }

    /** Trigger a local contamination event that lasts ~18 s (6 readings). */
    injectSpill() {
        this.spillTicksLeft = 6;
        this.demoStep();
    }

    demoStep() {
        this.demoTick += 3;
        const t = this.demoTick;
        const spill = this.spillTicksLeft > 0;
        if (spill) this.spillTicksLeft--;

        const ph = spill ? 4.85 : 7.3 + 0.25 * Math.sin(t * 0.05);
        const turb = spill ? 78.4 : 7.5 + 3.0 * Math.sin(t * 0.03);
        const doVal = spill ? 2.3 : 8.3 + 0.6 * Math.cos(t * 0.04);
        const temp = spill ? 27.8 : 19.2 + 1.2 * Math.sin(t * 0.02);
        const nis = spill ? 17.8 : 1.1 + 0.3 * Math.sin(t * 0.1);

        const reading = {
            timestamp: Math.floor(Date.now() / 1000),
            device_id: 1,
            latitude: 51.5074 + 0.001 * Math.sin(t * 0.02),
            longitude: -0.1278 + 0.0015 * Math.cos(t * 0.02),
            ph: +ph.toFixed(2),
            turbidity: +turb.toFixed(1),
            do_mg_l: +doVal.toFixed(2),
            temp_c: +temp.toFixed(2),
            battery_mv: spill ? 3820 : 3950,
            battery_pct: spill ? 65.0 : 78.0,
            solar_power_pct: spill ? 55 : 82,
            anomaly_flags: spill ? 0x1f : 0,
            ekf_nis: +nis.toFixed(2),
            is_anomaly: spill,
            anomaly_types: spill
                ? ['Severe Acidic Spike (pH 4.85)', 'Industrial Effluent Plume (78.4 NTU)', 'Severe Hypoxia (2.30 mg/L)']
                : []
        };

        if (this.onMessage) this.onMessage(reading);
    }
}

window.TelemetryWebSocket = TelemetryWebSocket;

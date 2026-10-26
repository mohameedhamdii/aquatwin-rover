/**
 * @file websocket.js
 * @brief Pure real-time hardware telemetry WebSocket client.
 * Connects to the backend gateway to receive live LoRa packets from physical nodes.
 * Strictly displays real telemetry — no mock generation or simulated data.
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
            console.log('[AquaTwin WS] Connected to live hardware telemetry gateway.');
            this.reconnectAttempts = 0;
            this.setStatus('live', 'REAL-TIME HARDWARE FEED');

            clearInterval(this.pingInterval);
            this.pingInterval = setInterval(() => {
                if (socket.readyState === WebSocket.OPEN) socket.send('ping');
            }, 15000);
        };

        socket.onmessage = (event) => {
            if (event.data === 'pong') return;
            try {
                const payload = JSON.parse(event.data);

                // Handle server status greetings
                if (payload.type === 'awaiting_telemetry') {
                    console.info('[AquaTwin WS]', payload.message);
                    this.setStatus('waiting', 'AWAITING NODE PACKET');
                    if (this.onMessage) this.onMessage({ awaiting: true, message: payload.message });
                    return;
                }

                // Legitimate telemetry packet from hardware
                this.setStatus('live', 'REAL-TIME HARDWARE FEED');
                if (this.onMessage) this.onMessage(payload);
            } catch (e) {
                console.error('[AquaTwin WS] Error parsing telemetry payload:', e);
            }
        };

        socket.onerror = (err) => console.warn('[AquaTwin WS] Socket error:', err);

        socket.onclose = () => {
            if (this.socket === socket) this.handleDisconnect();
        };
    }

    handleDisconnect() {
        clearInterval(this.pingInterval);
        this.reconnectAttempts++;
        const delay = Math.min(1000 * Math.pow(1.5, this.reconnectAttempts), this.maxReconnectDelay);

        this.setStatus('offline', 'OFFLINE / RECONNECTING');

        clearTimeout(this.reconnectTimer);
        this.reconnectTimer = setTimeout(() => this.connect(), delay);
    }
}

window.TelemetryWebSocket = TelemetryWebSocket;

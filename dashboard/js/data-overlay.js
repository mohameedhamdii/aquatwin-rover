/**
 * @file data-overlay.js
 * @brief Manages HUD gauges, Chart.js time-series streams, anomaly alerts & waypoints
 */

class DataOverlayManager {
    constructor() {
        this.chart = null;
        this.inAnomaly = false;
        this.calmReadings = 0;
        this.lastAnomalyKey = '';
        this.maxHistoryPoints = 30;
        this.activeMetricFilter = 'all';
        this.alertCount = 0;

        this.initChart();
        this.setupEventListeners();
        this.loadInitialWaypoints();
    }

    setupEventListeners() {
        // Chart tab filter buttons
        document.querySelectorAll('.btn-chart-tab').forEach(btn => {
            btn.addEventListener('click', (e) => {
                document.querySelectorAll('.btn-chart-tab').forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                this.activeMetricFilter = btn.dataset.metric;
                this.updateChartVisibility();
            });
        });

        // Banner dismiss button
        const btnDismiss = document.getElementById('btn-dismiss-banner');
        if (btnDismiss) {
            btnDismiss.addEventListener('click', () => {
                document.getElementById('anomaly-banner').classList.add('hidden');
            });
        }

        // Simulate Spill button
        const btnSpill = document.getElementById('btn-trigger-anomaly');
        if (btnSpill) {
            btnSpill.addEventListener('click', async () => {
                // fetch() only rejects on network errors, so a 404/501 from a static server must be checked too
                let ok = false;
                try {
                    const res = await fetch('/api/v1/simulate/anomaly', { method: 'POST' });
                    ok = res.ok;
                } catch (e) { /* no backend */ }
                if (!ok) window.dispatchEvent(new CustomEvent('aqua:mockSpill'));
            });
        }
    }

    initChart() {
        const canvas = document.getElementById('telemetryChart');
        if (!canvas || typeof Chart === 'undefined') {
            console.warn('[AquaTwin HUD] Chart.js or telemetryChart canvas unavailable.');
            return;
        }
        const ctx = canvas.getContext('2d');

        this.chart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [
                    {
                        label: 'pH',
                        data: [],
                        borderColor: '#00d2ff',
                        backgroundColor: 'rgba(0, 210, 255, 0.08)',
                        tension: 0.35,
                        borderWidth: 2,
                        pointRadius: 2,
                        yAxisID: 'y'
                    },
                    {
                        label: 'Turbidity (NTU)',
                        data: [],
                        borderColor: '#f59e0b',
                        backgroundColor: 'rgba(245, 158, 11, 0.08)',
                        tension: 0.35,
                        borderWidth: 2,
                        pointRadius: 2,
                        yAxisID: 'y1'
                    },
                    {
                        label: 'Dissolved O₂ (mg/L)',
                        data: [],
                        borderColor: '#10b981',
                        backgroundColor: 'rgba(16, 185, 129, 0.08)',
                        tension: 0.35,
                        borderWidth: 2,
                        pointRadius: 2,
                        yAxisID: 'y'
                    },
                    {
                        label: 'EKF NIS',
                        data: [],
                        borderColor: '#f43f5e',
                        backgroundColor: 'rgba(244, 63, 94, 0.08)',
                        tension: 0.35,
                        borderWidth: 2,
                        borderDash: [5, 5],
                        pointRadius: 2,
                        yAxisID: 'y1'
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: { duration: 400 },
                plugins: {
                    legend: {
                        display: true,
                        labels: {
                            color: '#94a3b8',
                            font: { size: 10, family: 'Inter' },
                            boxWidth: 10
                        }
                    },
                    tooltip: {
                        mode: 'index',
                        intersect: false,
                        backgroundColor: 'rgba(12, 22, 38, 0.95)',
                        borderColor: 'rgba(0, 210, 255, 0.3)',
                        borderWidth: 1,
                        titleColor: '#fff',
                        bodyColor: '#94a3b8'
                    }
                },
                scales: {
                    x: {
                        grid: { color: 'rgba(255, 255, 255, 0.04)' },
                        ticks: { color: '#64748b', font: { size: 9 }, maxTicksLimit: 6 }
                    },
                    y: {
                        type: 'linear',
                        position: 'left',
                        grid: { color: 'rgba(255, 255, 255, 0.04)' },
                        ticks: { color: '#00d2ff', font: { size: 9 } },
                        title: { display: true, text: 'pH / DO', color: '#00d2ff', font: { size: 9 } }
                    },
                    y1: {
                        type: 'linear',
                        position: 'right',
                        grid: { drawOnChartArea: false },
                        ticks: { color: '#f59e0b', font: { size: 9 } },
                        title: { display: true, text: 'Turbidity (NTU) / NIS', color: '#f59e0b', font: { size: 9 } }
                    }
                }
            }
        });
    }

    updateChartVisibility() {
        if (!this.chart) return;
        const ds = this.chart.data.datasets;

        if (this.activeMetricFilter === 'all') {
            ds.forEach(d => d.hidden = false);
        } else if (this.activeMetricFilter === 'ph') {
            ds.forEach(d => d.hidden = (d.label !== 'pH'));
        } else if (this.activeMetricFilter === 'turbidity') {
            ds.forEach(d => d.hidden = !d.label.includes('Turbidity'));
        } else if (this.activeMetricFilter === 'do') {
            ds.forEach(d => d.hidden = !d.label.includes('Dissolved O₂'));
        } else if (this.activeMetricFilter === 'nis') {
            ds.forEach(d => d.hidden = !d.label.includes('EKF NIS'));
        }
        this.chart.update('none');
    }

    paintMetric(key, value, decimals, max, state) {
        if (!Number.isFinite(value)) return;
        const labels = {
            ph:   { good: 'OPTIMAL', warn: 'CAUTION',  bad: 'ANOMALY' },
            turb: { good: 'CLEAR',   warn: 'MODERATE', bad: 'HIGH TURBIDITY' },
            do:   { good: 'HEALTHY', warn: 'LOW',      bad: 'HYPOXIC' },
            temp: { good: 'NORMAL',  warn: 'WARM',     bad: 'HEATED' }
        };
        const num = document.getElementById(`val-${key}`);
        const bar = document.getElementById(`bar-${key}`);
        const badge = document.getElementById(`badge-${key}`);
        const card = document.getElementById(`card-${key}`);
        if (num) num.textContent = value.toFixed(decimals);
        if (bar) bar.style.width = `${Math.min(100, Math.max(0, (value / max) * 100))}%`;
        if (badge) {
            badge.className = `metric-status status-${state}`;
            badge.textContent = labels[key][state];
        }
        if (card) card.dataset.state = state; // drives bar colour in CSS
    }

    updateTelemetryUI(reading) {
        const ph = Number(reading.ph);
        const turb = Number(reading.turbidity);
        const dox = Number(reading.do_mg_l);
        const temp = Number(reading.temp_c);

        this.paintMetric('ph', ph, 2, 14, (ph < 6 || ph > 9) ? 'bad' : (ph < 6.5 || ph > 8.5) ? 'warn' : 'good');
        this.paintMetric('turb', turb, 1, 50, turb > 50 ? 'bad' : turb > 25 ? 'warn' : 'good'); // bar spans 0–50 NTU
        this.paintMetric('do', dox, 2, 14, dox < 4 ? 'bad' : dox < 6.5 ? 'warn' : 'good');
        this.paintMetric('temp', temp, 1, 40, temp > 30 ? 'bad' : temp > 24 ? 'warn' : 'good');

        // Battery and solar
        const setText = (id, v) => { const el = document.getElementById(id); if (el && v !== undefined && v !== null) el.textContent = v; };
        setText('val-battery-pct', Math.round(reading.battery_pct));
        setText('val-battery-mv', reading.battery_mv);
        setText('val-solar-pct', reading.solar_power_pct);

        // EKF NIS badge & bar
        const nis = Number(reading.ekf_nis);
        const nisBadge = document.getElementById('nis-badge');
        const nisBar = document.getElementById('nis-meter-bar');
        if (Number.isFinite(nis)) {
            if (nisBadge) nisBadge.textContent = `NIS: ${nis.toFixed(2)}`;
            if (nisBar) nisBar.style.width = `${Math.min(100, (nis / 25.0) * 100)}%`;
        }

        // GPS
        const gpsCoords = document.getElementById('gps-coords-display');
        if (gpsCoords && Number.isFinite(reading.latitude) && Number.isFinite(reading.longitude)) {
            const latStr = `${Math.abs(reading.latitude).toFixed(4)}° ${reading.latitude >= 0 ? 'N' : 'S'}`;
            const lonStr = `${Math.abs(reading.longitude).toFixed(4)}° ${reading.longitude >= 0 ? 'E' : 'W'}`;
            gpsCoords.textContent = `${latStr}, ${lonStr}`;
        }

        // Timestamp
        const lastUpdate = document.getElementById('last-update-time');
        if (lastUpdate) lastUpdate.textContent = new Date(reading.timestamp * 1000).toLocaleTimeString();

        this.handleAnomalyState(reading);
        this.appendChartData(reading);
    }

    handleAnomalyState(reading) {
        const banner = document.getElementById('anomaly-banner');
        const bannerDesc = document.getElementById('banner-desc');

        if (reading.is_anomaly) {
            this.calmReadings = 0;
            const types = Array.isArray(reading.anomaly_types) ? reading.anomaly_types : [];
            const key = types.join('|') || 'nis';

            // Only react on onset or when the cause changes — not on every reading of a long event,
            // so the log isn't flooded and a dismissed banner stays dismissed.
            if (!this.inAnomaly || key !== this.lastAnomalyKey) {
                const reason = types.length ? types.join(', ') : 'Contamination detected by EKF innovation';
                if (bannerDesc) bannerDesc.textContent = `${reason} (NIS: ${Number(reading.ekf_nis).toFixed(2)} vs threshold 13.28).`;
                if (banner) banner.classList.remove('hidden');
                this.addAlertToLog(reading, types);
            }
            this.inAnomaly = true;
            this.lastAnomalyKey = key;
        } else if (this.inAnomaly && ++this.calmReadings >= 3) {
            // Clear the banner after three consecutive normal readings
            this.inAnomaly = false;
            this.lastAnomalyKey = '';
            if (banner) banner.classList.add('hidden');
        }
    }

    addAlertToLog(reading, types) {
        const feed = document.getElementById('alert-history');
        if (!feed) return;

        const emptyMsg = feed.querySelector('.empty-alerts');
        if (emptyMsg) emptyMsg.remove();

        this.alertCount++;
        const pill = document.getElementById('alert-count-pill');
        if (pill) pill.textContent = `${this.alertCount} Event${this.alertCount > 1 ? 's' : ''}`;

        // Built with textContent: anomaly strings come from the backend and must not be parsed as HTML
        const flags = Number(reading.anomaly_flags) || 0;
        const time = document.createElement('div');
        time.className = 'alert-item-time';
        time.textContent = `${new Date(reading.timestamp * 1000).toLocaleTimeString()} • Anomaly Flags: 0x${flags.toString(16).toUpperCase()}`;

        const msg = document.createElement('div');
        msg.className = 'alert-item-msg';
        msg.textContent = types.length ? types.join(' & ') : 'Statistical NIS Deviation';

        const item = document.createElement('div');
        item.className = 'alert-item';
        item.append(time, msg);
        feed.insertBefore(item, feed.firstChild);

        while (feed.children.length > 10) feed.removeChild(feed.lastChild);
    }

    appendChartData(reading) {
        if (!this.chart) return;

        const timeLabel = new Date(reading.timestamp * 1000).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });

        this.chart.data.labels.push(timeLabel);
        this.chart.data.datasets[0].data.push(reading.ph);
        this.chart.data.datasets[1].data.push(reading.turbidity);
        this.chart.data.datasets[2].data.push(reading.do_mg_l);
        this.chart.data.datasets[3].data.push(reading.ekf_nis);

        if (this.chart.data.labels.length > this.maxHistoryPoints) {
            this.chart.data.labels.shift();
            this.chart.data.datasets.forEach(ds => ds.data.shift());
        }

        this.chart.update('none');
    }

    async loadInitialWaypoints() {
        let waypoints = [
            { id: 0, label: "Base Dock", target_reached: true },
            { id: 1, label: "North Inflow Zone", target_reached: false },
            { id: 2, label: "Runoff Outfall", target_reached: false },
            { id: 3, label: "Deep Center Basin", target_reached: false },
            { id: 4, label: "South Spillway", target_reached: false }
        ];

        try {
            const res = await fetch('/api/v1/waypoints');
            if (res.ok) {
                const data = await res.json();
                if (data && data.length > 0) waypoints = data;
            }
        } catch (e) {
            // Using default waypoints
        }

        const list = document.getElementById('waypoint-list');
        if (!list) return;

        const currentIdx = waypoints.findIndex(wp => !wp.target_reached);
        list.replaceChildren(...waypoints.map((wp, idx) => {
            const item = document.createElement('div');
            item.className = `waypoint-item${idx === currentIdx ? ' current' : ''}`;

            const name = document.createElement('div');
            name.className = 'wp-name';
            name.textContent = `WP-${(Number.isFinite(wp.id) ? wp.id : idx) + 1}: ${wp.label}`;

            const status = document.createElement('div');
            status.className = `wp-status ${wp.target_reached ? 'done' : 'active'}`;
            status.textContent = wp.target_reached ? 'PASSED' : 'PATROL';

            item.append(name, status);
            return item;
        }));
    }
}

window.DataOverlayManager = DataOverlayManager;

/**
 * @file data-overlay.js
 * @brief Manages HUD gauges, Chart.js time-series streams, anomaly alerts & waypoints
 */

class DataOverlayManager {
    constructor() {
        this.chart = null;
        this.historyData = {
            labels: [],
            ph: [],
            turbidity: [],
            do: [],
            temp: [],
            nis: []
        };
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
                try {
                    await fetch('/api/v1/simulate/anomaly', { method: 'POST' });
                } catch (e) {
                    console.log('Dispatching client-side simulated spill');
                    // Fallback to local dispatch
                    window.dispatchEvent(new CustomEvent('aqua:mockSpill'));
                }
            });
        }
    }

    initChart() {
        const ctx = document.getElementById('telemetryChart').getContext('2d');

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

    updateTelemetryUI(reading) {
        // 1. pH card
        const valPh = document.getElementById('val-ph');
        const barPh = document.getElementById('bar-ph');
        const badgePh = document.getElementById('badge-ph');
        if (valPh) valPh.textContent = Number(reading.ph).toFixed(2);
        if (barPh) barPh.style.width = `${Math.min(100, Math.max(0, (reading.ph / 14) * 100))}%`;
        if (badgePh) {
            if (reading.ph < 6.0 || reading.ph > 9.0) {
                badgePh.className = 'metric-status status-bad';
                badgePh.textContent = 'ANOMALY';
            } else if (reading.ph < 6.5 || reading.ph > 8.5) {
                badgePh.className = 'metric-status status-warn';
                badgePh.textContent = 'CAUTION';
            } else {
                badgePh.className = 'metric-status status-good';
                badgePh.textContent = 'OPTIMAL';
            }
        }

        // 2. Turbidity card
        const valTurb = document.getElementById('val-turb');
        const barTurb = document.getElementById('bar-turb');
        const badgeTurb = document.getElementById('badge-turb');
        if (valTurb) valTurb.textContent = Number(reading.turbidity).toFixed(1);
        if (barTurb) barTurb.style.width = `${Math.min(100, Math.max(0, (reading.turbidity / 100) * 100))}%`;
        if (badgeTurb) {
            if (reading.turbidity > 50.0) {
                badgeTurb.className = 'metric-status status-bad';
                badgeTurb.textContent = 'HIGH TURBIDITY';
            } else if (reading.turbidity > 25.0) {
                badgeTurb.className = 'metric-status status-warn';
                badgeTurb.textContent = 'MODERATE';
            } else {
                badgeTurb.className = 'metric-status status-good';
                badgeTurb.textContent = 'CLEAR';
            }
        }

        // 3. Dissolved Oxygen card
        const valDo = document.getElementById('val-do');
        const barDo = document.getElementById('bar-do');
        const badgeDo = document.getElementById('badge-do');
        if (valDo) valDo.textContent = Number(reading.do_mg_l).toFixed(2);
        if (barDo) barDo.style.width = `${Math.min(100, Math.max(0, (reading.do_mg_l / 14) * 100))}%`;
        if (badgeDo) {
            if (reading.do_mg_l < 4.0) {
                badgeDo.className = 'metric-status status-bad';
                badgeDo.textContent = 'HYPOXIC';
            } else if (reading.do_mg_l < 6.5) {
                badgeDo.className = 'metric-status status-warn';
                badgeDo.textContent = 'LOW';
            } else {
                badgeDo.className = 'metric-status status-good';
                badgeDo.textContent = 'HEALTHY';
            }
        }

        // 4. Temperature card
        const valTemp = document.getElementById('val-temp');
        const barTemp = document.getElementById('bar-temp');
        if (valTemp) valTemp.textContent = Number(reading.temp_c).toFixed(1);
        if (barTemp) barTemp.style.width = `${Math.min(100, Math.max(0, (reading.temp_c / 40) * 100))}%`;

        // 5. Battery and Solar
        const valBattPct = document.getElementById('val-battery-pct');
        const valBattMv = document.getElementById('val-battery-mv');
        const valSolarPct = document.getElementById('val-solar-pct');
        if (valBattPct) valBattPct.textContent = Math.round(reading.battery_pct);
        if (valBattMv) valBattMv.textContent = reading.battery_mv;
        if (valSolarPct) valSolarPct.textContent = reading.solar_power_pct;

        // 6. EKF NIS Badge & Bar
        const nisBadge = document.getElementById('nis-badge');
        const nisBar = document.getElementById('nis-meter-bar');
        if (nisBadge) nisBadge.textContent = `NIS: ${Number(reading.ekf_nis).toFixed(2)}`;
        if (nisBar) {
            const pct = Math.min(100, (reading.ekf_nis / 25.0) * 100);
            nisBar.style.width = `${pct}%`;
        }

        // 7. GPS Display
        const gpsCoords = document.getElementById('gps-coords-display');
        if (gpsCoords) {
            const latStr = `${Math.abs(reading.latitude).toFixed(4)}° ${reading.latitude >= 0 ? 'N' : 'S'}`;
            const lonStr = `${Math.abs(reading.longitude).toFixed(4)}° ${reading.longitude >= 0 ? 'E' : 'W'}`;
            gpsCoords.textContent = `${latStr}, ${lonStr}`;
        }

        // 8. Timestamp
        const lastUpdate = document.getElementById('last-update-time');
        if (lastUpdate) {
            const d = new Date(reading.timestamp * 1000);
            lastUpdate.textContent = d.toLocaleTimeString();
        }

        // 9. Anomaly Banner & Log handling
        this.handleAnomalyState(reading);

        // 10. Append to time-series chart
        this.appendChartData(reading);
    }

    handleAnomalyState(reading) {
        const banner = document.getElementById('anomaly-banner');
        const bannerDesc = document.getElementById('banner-desc');

        if (reading.is_anomaly) {
            if (banner) {
                banner.classList.remove('hidden');
                const reason = reading.anomaly_types && reading.anomaly_types.length > 0
                    ? reading.anomaly_types.join(', ')
                    : 'Contamination detected by EKF innovation';
                if (bannerDesc) bannerDesc.textContent = `${reason} (NIS: ${Number(reading.ekf_nis).toFixed(2)} vs threshold 13.28).`;
            }
            this.addAlertToLog(reading);
        }
    }

    addAlertToLog(reading) {
        const feed = document.getElementById('alert-history');
        if (!feed) return;

        // Remove empty state placeholder if present
        const emptyMsg = feed.querySelector('.empty-alerts');
        if (emptyMsg) emptyMsg.remove();

        this.alertCount++;
        const pill = document.getElementById('alert-count-pill');
        if (pill) pill.textContent = `${this.alertCount} Event${this.alertCount > 1 ? 's' : ''}`;

        const alertItem = document.createElement('div');
        alertItem.className = 'alert-item';
        const d = new Date(reading.timestamp * 1000);
        const timeStr = d.toLocaleTimeString();

        const reasons = reading.anomaly_types && reading.anomaly_types.length > 0
            ? reading.anomaly_types.join(' & ')
            : 'Statistical NIS Deviation';

        alertItem.innerHTML = `
            <div class="alert-item-time">${timeStr} • Anomaly Flags: 0x${reading.anomaly_flags.toString(16).toUpperCase()}</div>
            <div class="alert-item-msg">${reasons}</div>
        `;

        feed.insertBefore(alertItem, feed.firstChild);

        // Limit to 10 visible alerts
        if (feed.children.length > 10) {
            feed.removeChild(feed.lastChild);
        }
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

        list.innerHTML = '';
        waypoints.forEach((wp, idx) => {
            const item = document.createElement('div');
            item.className = `waypoint-item ${idx === 1 ? 'current' : ''}`;
            item.innerHTML = `
                <div class="wp-name">WP-${wp.id + 1}: ${wp.label}</div>
                <div class="wp-status ${wp.target_reached ? 'done' : 'active'}">
                    ${wp.target_reached ? 'PASSED' : 'PATROL'}
                </div>
            `;
            list.appendChild(item);
        });
    }
}

window.DataOverlayManager = DataOverlayManager;

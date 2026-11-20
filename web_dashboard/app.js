/**
 * ESP32 Air Quality & Environmental Sensing Hub
 * Real-Time Telemetry, Dynamic Charts, Warning System & Hardware Synchronization
 */

// Application State
const state = {
  isDemoMode: true,
  isConnected: false,
  audioMuted: false,
  endpoint: '192.168.4.1', // Default SoftAP IP
  pollInterval: 2000,
  timerId: null,
  warmupRemaining: 45, // Seconds
  warmupTimerId: null,

  thresholds: {
    warn: 1200,
    alert: 2000,
    voltageDividerRatio: 1.50
  },

  currentData: {
    gasAdc: 1280,
    gasPpm: 415,
    gasVolt: 1.03,
    tempC: 26.8,
    tempF: 80.2,
    humidity: 54,
    heatIndex: 27.4,
    dewPoint: 16.8,
    comfort: 'Pleasant',
    status: 'NORMAL', // NORMAL | WARNING | ALERT
    greenLed: true,
    redLed: false,
    buzzer: false,
    oledReady: true,
    timestamp: new Date().toLocaleTimeString()
  },

  history: []
};

// Audio Synthesizer for Alert Beep
let audioCtx = null;
function playAlarmBeep() {
  if (state.audioMuted) return;
  try {
    if (!audioCtx) {
      audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    }
    if (audioCtx.state === 'suspended') {
      audioCtx.resume();
    }

    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();

    osc.type = 'sawtooth';
    osc.frequency.setValueAtTime(880, audioCtx.currentTime); // A5 note
    osc.frequency.exponentialRampToValueAtTime(440, audioCtx.currentTime + 0.25);

    gain.gain.setValueAtTime(0.15, audioCtx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.01, audioCtx.currentTime + 0.25);

    osc.connect(gain);
    gain.connect(audioCtx.destination);

    osc.start();
    osc.stop(audioCtx.currentTime + 0.25);
  } catch (e) {
    console.warn('Audio playback not permitted yet:', e);
  }
}

// DOM Elements
const el = {
  connIndicator: document.getElementById('connIndicator'),
  connStatusText: document.getElementById('connStatusText'),
  deviceSelector: document.getElementById('deviceSelector'),
  espIpInput: document.getElementById('espIpInput'),
  btnConnect: document.getElementById('btnConnect'),
  btnToggleDemo: document.getElementById('btnToggleDemo'),
  demoBtnText: document.getElementById('demoBtnText'),
  btnToggleAudio: document.getElementById('btnToggleAudio'),
  audioIcon: document.getElementById('audioIcon'),
  btnOpenPinout: document.getElementById('btnOpenPinout'),
  pinoutModal: document.getElementById('pinoutModal'),
  btnCloseModal: document.getElementById('btnCloseModal'),

  warmupBanner: document.getElementById('warmupBanner'),
  warmupCountdown: document.getElementById('warmupCountdown'),
  warmupText: document.getElementById('warmupText'),

  statusBanner: document.getElementById('statusBanner'),
  bannerIcon: document.getElementById('bannerIcon'),
  bannerTitle: document.getElementById('bannerTitle'),
  bannerDesc: document.getElementById('bannerDesc'),
  bannerStateTag: document.getElementById('bannerStateTag'),

  gasPpm: document.getElementById('gasPpm'),
  gasAdc: document.getElementById('gasAdc'),
  gasVolt: document.getElementById('gasVolt'),
  gasGaugeFill: document.getElementById('gasGaugeFill'),
  gasQualityBadge: document.getElementById('gasQualityBadge'),
  gasUpdated: document.getElementById('gasUpdated'),
  cardGas: document.getElementById('cardGas'),

  tempVal: document.getElementById('tempVal'),
  tempFVal: document.getElementById('tempFVal'),
  heatIndexVal: document.getElementById('heatIndexVal'),
  tempGaugeFill: document.getElementById('tempGaugeFill'),
  tempBadge: document.getElementById('tempBadge'),

  humVal: document.getElementById('humVal'),
  dewPointVal: document.getElementById('dewPointVal'),
  comfortLevel: document.getElementById('comfortLevel'),
  humGaugeFill: document.getElementById('humGaugeFill'),
  humBadge: document.getElementById('humBadge'),

  ledGreenIndicator: document.getElementById('ledGreenIndicator'),
  greenLedState: document.getElementById('greenLedState'),
  ledRedIndicator: document.getElementById('ledRedIndicator'),
  redLedState: document.getElementById('redLedState'),
  buzzerIcon: document.getElementById('buzzerIcon'),
  buzzerState: document.getElementById('buzzerState'),
  btnTestAlarm: document.getElementById('btnTestAlarm'),

  warnThresholdSlider: document.getElementById('warnThresholdSlider'),
  warnThresholdDisplay: document.getElementById('warnThresholdDisplay'),
  alertThresholdSlider: document.getElementById('alertThresholdSlider'),
  alertThresholdDisplay: document.getElementById('alertThresholdDisplay'),
  pollIntervalSelect: document.getElementById('pollIntervalSelect'),
  dividerRatioInput: document.getElementById('dividerRatioInput'),
  dividerRatioDisplay: document.getElementById('dividerRatioDisplay'),
  btnSaveThresholds: document.getElementById('btnSaveThresholds'),
  legendWarnVal: document.getElementById('legendWarnVal'),
  legendAlertVal: document.getElementById('legendAlertVal'),

  telemetryTableBody: document.getElementById('telemetryTableBody'),
  btnClearLog: document.getElementById('btnClearLog'),
  btnExportCsv: document.getElementById('btnExportCsv')
};

// Charts Initialization
let airQualityChart, tempHumChart;

function initCharts() {
  const commonOptions = {
    responsive: true,
    maintainAspectRatio: false,
    animation: { duration: 400 },
    plugins: {
      legend: { display: false }
    },
    scales: {
      x: {
        grid: { color: 'rgba(255, 255, 255, 0.05)' },
        ticks: { color: '#9ca3af', font: { size: 10, family: 'JetBrains Mono' } }
      },
      y: {
        grid: { color: 'rgba(255, 255, 255, 0.05)' },
        ticks: { color: '#9ca3af', font: { size: 10, family: 'JetBrains Mono' } }
      }
    }
  };

  // 1. Air Quality Chart
  const ctxGas = document.getElementById('airQualityChart').getContext('2d');
  airQualityChart = new Chart(ctxGas, {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        {
          label: 'Gas Level (ADC)',
          data: [],
          borderColor: '#10b981',
          backgroundColor: 'rgba(16, 185, 129, 0.12)',
          fill: true,
          tension: 0.35,
          borderWidth: 2,
          pointRadius: 3,
          pointBackgroundColor: '#10b981'
        },
        {
          label: 'Warning Line',
          data: [],
          borderColor: '#f59e0b',
          borderDash: [5, 5],
          pointRadius: 0,
          borderWidth: 1.5,
          fill: false
        },
        {
          label: 'Alert Line',
          data: [],
          borderColor: '#ef4444',
          borderDash: [5, 5],
          pointRadius: 0,
          borderWidth: 1.5,
          fill: false
        }
      ]
    },
    options: {
      ...commonOptions,
      scales: {
        ...commonOptions.scales,
        y: {
          ...commonOptions.scales.y,
          min: 0,
          suggestedMax: 3000,
          title: { display: true, text: 'ADC Value (0-4095)', color: '#6b7280' }
        }
      }
    }
  });

  // 2. Temp & Humidity Chart
  const ctxTemp = document.getElementById('tempHumChart').getContext('2d');
  tempHumChart = new Chart(ctxTemp, {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        {
          label: 'Temperature (°C)',
          data: [],
          borderColor: '#06b6d4',
          yAxisID: 'yTemp',
          tension: 0.35,
          borderWidth: 2,
          pointRadius: 2.5,
          pointBackgroundColor: '#06b6d4'
        },
        {
          label: 'Humidity (%)',
          data: [],
          borderColor: '#3b82f6',
          yAxisID: 'yHum',
          tension: 0.35,
          borderWidth: 2,
          pointRadius: 2.5,
          pointBackgroundColor: '#3b82f6'
        }
      ]
    },
    options: {
      ...commonOptions,
      scales: {
        x: commonOptions.scales.x,
        yTemp: {
          type: 'linear',
          position: 'left',
          grid: { color: 'rgba(255, 255, 255, 0.05)' },
          ticks: { color: '#06b6d4' },
          title: { display: true, text: 'Temp (°C)', color: '#06b6d4' },
          suggestedMin: 15,
          suggestedMax: 45
        },
        yHum: {
          type: 'linear',
          position: 'right',
          grid: { drawOnChartArea: false },
          ticks: { color: '#3b82f6' },
          title: { display: true, text: 'Humidity (%)', color: '#3b82f6' },
          suggestedMin: 20,
          suggestedMax: 100
        }
      }
    }
  });
}

// Update charts with new data point
function updateCharts(data) {
  const timeLabel = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });

  // Update Gas Chart
  airQualityChart.data.labels.push(timeLabel);
  airQualityChart.data.datasets[0].data.push(data.gasAdc);
  airQualityChart.data.datasets[1].data.push(state.thresholds.warn);
  airQualityChart.data.datasets[2].data.push(state.thresholds.alert);

  // Dynamic border color based on value
  if (data.gasAdc >= state.thresholds.alert) {
    airQualityChart.data.datasets[0].borderColor = '#ef4444';
    airQualityChart.data.datasets[0].pointBackgroundColor = '#ef4444';
  } else if (data.gasAdc >= state.thresholds.warn) {
    airQualityChart.data.datasets[0].borderColor = '#f59e0b';
    airQualityChart.data.datasets[0].pointBackgroundColor = '#f59e0b';
  } else {
    airQualityChart.data.datasets[0].borderColor = '#10b981';
    airQualityChart.data.datasets[0].pointBackgroundColor = '#10b981';
  }

  // Keep max 20 points
  if (airQualityChart.data.labels.length > 20) {
    airQualityChart.data.labels.shift();
    airQualityChart.data.datasets.forEach(ds => ds.data.shift());
  }
  airQualityChart.update('none');

  // Update Temp & Hum Chart
  tempHumChart.data.labels.push(timeLabel);
  tempHumChart.data.datasets[0].data.push(data.tempC);
  tempHumChart.data.datasets[1].data.push(data.humidity);

  if (tempHumChart.data.labels.length > 20) {
    tempHumChart.data.labels.shift();
    tempHumChart.data.datasets.forEach(ds => ds.data.shift());
  }
  tempHumChart.update('none');
}

// Calculate Derived Environmental Indices
function calculateIndices(tempC, humidity) {
  const tempF = (tempC * 9 / 5) + 32;

  // Heat Index approximation (Rothfusz equation)
  const hi = -42.379 + 2.04901523 * tempF + 10.14333127 * humidity -
             0.22475541 * tempF * humidity - 0.00683783 * tempF * tempF -
             0.05481717 * humidity * humidity + 0.00122874 * tempF * tempF * humidity +
             0.00085282 * tempF * humidity * humidity - 0.00000199 * tempF * tempF * humidity * humidity;
  const hiC = (hi - 32) * 5 / 9;

  // Dew point approximation (Magnus formula)
  const a = 17.27, b = 237.7;
  const alpha = ((a * tempC) / (b + tempC)) + Math.log(humidity / 100.0);
  const dewPoint = (b * alpha) / (a - alpha);

  let comfort = 'Comfortable';
  if (humidity < 35) comfort = 'Dry Air';
  else if (humidity > 70) comfort = 'Muggy / Humid';
  else if (tempC > 30) comfort = 'Hot & Sticky';
  else comfort = 'Optimal';

  return {
    tempF: Number(tempF.toFixed(1)),
    heatIndex: Number(hiC.toFixed(1)),
    dewPoint: Number(dewPoint.toFixed(1)),
    comfort
  };
}

// Convert ADC to Voltage & Estimated PPM for MQ-135
function convertGas(adcVal, ratio) {
  // ESP32 ADC is 12-bit (0-4095), reference is 3.3V
  const measuredV = (adcVal / 4095.0) * 3.3;
  // Account for voltage divider
  const sensorV = measuredV * ratio;

  // Exponential approximation for PPM eq. in clean-to-polluted air
  // Baseline ~400 PPM (fresh atmospheric CO2 / air)
  const ppm = Math.round(400 + Math.pow(adcVal / 600.0, 2.5) * 80);

  return {
    measuredV: Number(measuredV.toFixed(2)),
    sensorV: Number(sensorV.toFixed(2)),
    ppm: Math.min(ppm, 9999)
  };
}

// Process and Render Data on Dashboard UI
function renderTelemetry(data) {
  state.currentData = { ...state.currentData, ...data };
  const d = state.currentData;

  // 1. Gas Metric Card
  el.gasPpm.textContent = d.gasPpm;
  el.gasAdc.textContent = d.gasAdc;
  el.gasVolt.textContent = d.gasVolt;
  el.gasUpdated.textContent = new Date().toLocaleTimeString();

  // Percentage for progress bar (capped at 100%)
  const gasPct = Math.min(Math.round((d.gasAdc / 4095) * 100), 100);
  el.gasGaugeFill.style.width = `${gasPct}%`;

  // Status identification logic
  let status = 'NORMAL';
  if (d.gasAdc >= state.thresholds.alert) {
    status = 'ALERT';
  } else if (d.gasAdc >= state.thresholds.warn) {
    status = 'WARNING';
  }
  d.status = status;

  // Hardware state assignment
  if (status === 'ALERT') {
    d.greenLed = false;
    d.redLed = true;
    d.buzzer = true;
  } else if (status === 'WARNING') {
    d.greenLed = false;
    d.redLed = true;
    d.buzzer = false;
  } else {
    d.greenLed = true;
    d.redLed = false;
    d.buzzer = false;
  }

  // Update Status Banner
  el.statusBanner.className = 'alert-banner';
  el.cardGas.className = 'metric-card card-gas';
  el.gasGaugeFill.className = 'gauge-bar-fill';

  if (status === 'ALERT') {
    el.statusBanner.classList.add('banner-danger');
    el.cardGas.classList.add('card-danger');
    el.gasGaugeFill.classList.add('fill-danger');
    el.bannerIcon.className = 'fa-solid fa-triangle-exclamation banner-icon';
    el.bannerTitle.textContent = 'CRITICAL ALERT: HARMFUL GAS / SMOKE LEVEL!';
    el.bannerDesc.textContent = 'Air quality threshold severely exceeded! Buzzer is sounding and Red LED is active. Immediate ventilation required!';
    el.bannerStateTag.textContent = 'CRITICAL';
    el.gasQualityBadge.className = 'badge badge-danger';
    el.gasQualityBadge.textContent = 'HAZARDOUS';
    playAlarmBeep();
  } else if (status === 'WARNING') {
    el.statusBanner.classList.add('banner-warning');
    el.cardGas.classList.add('card-warning');
    el.gasGaugeFill.classList.add('fill-warn');
    el.bannerIcon.className = 'fa-solid fa-circle-exclamation banner-icon';
    el.bannerTitle.textContent = 'AIR QUALITY WARNING: ELEVATED GAS CONCENTRATION';
    el.bannerDesc.textContent = 'Predefined warning threshold crossed. Red LED active. Check room ventilation and windows.';
    el.bannerStateTag.textContent = 'WARNING';
    el.gasQualityBadge.className = 'badge badge-warning';
    el.gasQualityBadge.textContent = 'MODERATE';
  } else {
    el.statusBanner.classList.add('banner-normal');
    el.bannerIcon.className = 'fa-solid fa-circle-check banner-icon';
    el.bannerTitle.textContent = 'AIR QUALITY STATUS: NORMAL & HEALTHY';
    el.bannerDesc.textContent = 'All environmental parameters (MQ-135, Temperature, Humidity) are well within safe baseline limits.';
    el.bannerStateTag.textContent = 'NORMAL';
    el.gasQualityBadge.className = 'badge badge-normal';
    el.gasQualityBadge.textContent = 'EXCELLENT';
  }

  // 2. Temperature Metric Card
  el.tempVal.textContent = d.tempC;
  el.tempFVal.textContent = d.tempF;
  el.heatIndexVal.textContent = d.heatIndex;
  const tempPct = Math.min(Math.max(Math.round((d.tempC / 50) * 100), 5), 100);
  el.tempGaugeFill.style.width = `${tempPct}%`;

  if (d.tempC > 34) {
    el.tempBadge.className = 'badge badge-warning';
    el.tempBadge.textContent = 'HOT';
  } else if (d.tempC < 18) {
    el.tempBadge.className = 'badge badge-warning';
    el.tempBadge.textContent = 'COOL';
  } else {
    el.tempBadge.className = 'badge badge-normal';
    el.tempBadge.textContent = 'COMFORTABLE';
  }

  // 3. Humidity Metric Card
  el.humVal.textContent = d.humidity;
  el.dewPointVal.textContent = d.dewPoint;
  el.comfortLevel.textContent = d.comfort;
  el.humGaugeFill.style.width = `${Math.min(d.humidity, 100)}%`;

  if (d.humidity < 35) {
    el.humBadge.className = 'badge badge-warning';
    el.humBadge.textContent = 'DRY';
  } else if (d.humidity > 68) {
    el.humBadge.className = 'badge badge-warning';
    el.humBadge.textContent = 'HUMID';
  } else {
    el.humBadge.className = 'badge badge-normal';
    el.humBadge.textContent = 'OPTIMAL';
  }

  // 4. Actuators & Output State
  if (d.greenLed) {
    if (el.ledGreenIndicator) el.ledGreenIndicator.classList.remove('off');
    if (el.greenLedState) {
      el.greenLedState.className = 'state-pill state-on';
      el.greenLedState.textContent = 'ON';
    }
  } else {
    if (el.ledGreenIndicator) el.ledGreenIndicator.classList.add('off');
    if (el.greenLedState) {
      el.greenLedState.className = 'state-pill state-off';
      el.greenLedState.textContent = 'OFF';
    }
  }

  if (d.redLed) {
    if (el.ledRedIndicator) el.ledRedIndicator.classList.remove('off');
    if (el.redLedState) {
      el.redLedState.className = 'state-pill state-on';
      el.redLedState.style.background = 'rgba(239, 68, 68, 0.25)';
      el.redLedState.style.color = '#f87171';
      el.redLedState.textContent = 'ON';
    }
  } else {
    if (el.ledRedIndicator) el.ledRedIndicator.classList.add('off');
    if (el.redLedState) {
      el.redLedState.className = 'state-pill state-off';
      el.redLedState.style.background = '';
      el.redLedState.style.color = '';
      el.redLedState.textContent = 'OFF';
    }
  }

  if (d.buzzer) {
    el.buzzerIcon.classList.add('alarm');
    el.buzzerState.className = 'state-pill state-on';
    el.buzzerState.style.background = 'rgba(239, 68, 68, 0.25)';
    el.buzzerState.style.color = '#f87171';
    el.buzzerState.textContent = 'SOUNDING';
  } else {
    el.buzzerIcon.classList.remove('alarm');
    el.buzzerState.className = 'state-pill state-off';
    el.buzzerState.style.background = '';
    el.buzzerState.style.color = '';
    el.buzzerState.textContent = 'SILENT';
  }

  // Update Charts
  updateCharts(d);

  // Append to Telemetry Log Table
  appendLogEntry(d);
}

// Append new row to telemetry log table
function appendLogEntry(d) {
  const tr = document.createElement('tr');
  const statusBadge = d.status === 'ALERT' ? '<span class="badge badge-danger">ALERT</span>' :
                      d.status === 'WARNING' ? '<span class="badge badge-warning">WARN</span>' :
                      '<span class="badge badge-normal">NORMAL</span>';

  const actuators = `${d.greenLed ? '🟢Grn' : '⚪'} ${d.redLed ? '🔴Red' : '⚪'} ${d.buzzer ? '🔊Bz' : '🔇'}`;

  tr.innerHTML = `
    <td>${new Date().toLocaleTimeString()}</td>
    <td class="font-mono">${d.gasAdc}</td>
    <td class="font-mono">${d.gasPpm}</td>
    <td class="font-mono">${d.gasVolt}V</td>
    <td class="font-mono">${d.tempC}°C</td>
    <td class="font-mono">${d.humidity}%</td>
    <td>${statusBadge}</td>
    <td>${actuators}</td>
  `;

  // Prepend to top
  el.telemetryTableBody.insertBefore(tr, el.telemetryTableBody.firstChild);

  // Keep max 20 rows in table
  while (el.telemetryTableBody.children.length > 20) {
    el.telemetryTableBody.removeChild(el.telemetryTableBody.lastChild);
  }

  // Store in memory for CSV export
  state.history.push({
    timestamp: new Date().toISOString(),
    gasAdc: d.gasAdc,
    gasPpm: d.gasPpm,
    voltage: d.gasVolt,
    tempC: d.tempC,
    humidity: d.humidity,
    status: d.status,
    greenLed: d.greenLed ? 1 : 0,
    redLed: d.redLed ? 1 : 0,
    buzzer: d.buzzer ? 1 : 0
  });
}

// Simulation / Demo Mode Generator
let demoTick = 0;
function generateDemoData() {
  demoTick++;
  // Create natural sinusoidal variation with random noise
  const baseAdc = 950;
  const wave = Math.sin(demoTick / 5) * 200;
  const noise = (Math.random() - 0.5) * 150;
  
  // Occasionally simulate a smoke/gas spike every 15-20 ticks for demonstration
  let spike = 0;
  if (demoTick % 16 === 10) {
    spike = 700; // Trigger warning
  } else if (demoTick % 16 === 11 || demoTick % 16 === 12) {
    spike = 1350; // Trigger alert
  }

  const simulatedAdc = Math.min(Math.max(Math.round(baseAdc + wave + noise + spike), 400), 3800);
  const gasProps = convertGas(simulatedAdc, state.thresholds.voltageDividerRatio);

  const simulatedTemp = Number((26.5 + Math.sin(demoTick / 10) * 1.5 + (Math.random() - 0.5) * 0.3).toFixed(1));
  const simulatedHum = Math.round(52 + Math.cos(demoTick / 8) * 6 + (Math.random() - 0.5) * 2);
  const indices = calculateIndices(simulatedTemp, simulatedHum);

  renderTelemetry({
    gasAdc: simulatedAdc,
    gasPpm: gasProps.ppm,
    gasVolt: gasProps.measuredV,
    tempC: simulatedTemp,
    humidity: simulatedHum,
    ...indices
  });
}

// Populate Device Selector (Multi-ESP32 Support)
async function refreshDeviceList() {
  if (!el.deviceSelector || !window.location.protocol.startsWith('http')) return;
  try {
    const res = await fetch('/api/devices');
    if (!res.ok) return;
    const devices = await res.json();
    const currentVal = el.deviceSelector.value;
    el.deviceSelector.innerHTML = '<option value="" style="background:#0f172a; color:#f9fafb;">All Devices (Auto)</option>';
    devices.forEach(d => {
      const opt = document.createElement('option');
      opt.value = d.id;
      opt.textContent = `${d.id} [${d.status}]`;
      opt.style.background = '#0f172a';
      opt.style.color = '#f9fafb';
      if (d.id === currentVal) opt.selected = true;
      el.deviceSelector.appendChild(opt);
    });
  } catch (e) {
    // Silently handle if offline
  }
}

// Fetch live telemetry from REST Endpoint (Local ESP32 or Cloud Server)
async function fetchEsp32Data() {
  const customIp = el.espIpInput.value.trim();
  let apiEndpoint = '/api/data';

  if (customIp) {
    let target = customIp;
    if (!target.startsWith('http://') && !target.startsWith('https://')) {
      target = 'http://' + target;
    }
    apiEndpoint = `${target}/api/data`;
  } else if (window.location.protocol.startsWith('http')) {
    // Hosted on Render / Cloud or local server: fetch from relative API
    apiEndpoint = state.selectedDevice ? `/api/data?device=${encodeURIComponent(state.selectedDevice)}` : '/api/data';
  } else {
    apiEndpoint = `http://${state.endpoint}/api/data`;
  }

  try {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 3500);

    const response = await fetch(apiEndpoint, { signal: controller.signal });
    clearTimeout(timeoutId);

    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const json = await response.json();

    // Set connection status
    const label = customIp ? `Connected: ${customIp}` : (window.location.hostname || 'Cloud Server');
    setConnectionState(true, `Connected: ${label}`);

    // Parse incoming payload: { adc: 2450, temp: 32.5, hum: 56, green: 1, red: 0, buzz: 0 }
    const gasProps = convertGas(json.adc, state.thresholds.voltageDividerRatio);
    const tempVal = (json.temp !== undefined && json.temp !== null) ? Number(json.temp) : 0.0;
    const humVal = (json.hum !== undefined && json.hum !== null) ? Number(json.hum) : 0.0;
    const indices = calculateIndices(tempVal, humVal);

    renderTelemetry({
      gasAdc: json.adc,
      gasPpm: json.ppm || gasProps.ppm,
      gasVolt: json.voltage || gasProps.measuredV,
      tempC: tempVal,
      humidity: humVal,
      greenLed: Boolean(json.green),
      redLed: Boolean(json.red),
      buzzer: Boolean(json.buzz),
      ...indices
    });

  } catch (err) {
    console.warn('Telemetry poll failed:', err.message);

    // Only fallback to demo data if the user has demo mode turned on
    if (state.isDemoMode) {
      setConnectionState(false, 'Demo Mode (Simulated)');
      generateDemoData();
    } else {
      if (customIp && window.location.protocol === 'https:' && customIp.startsWith('http://')) {
        setConnectionState(false, 'Mixed Content Block: Open http://' + customIp + ' in a new tab');
      } else {
        setConnectionState(false, 'Waiting for ESP32 telemetry...');
      }
    }
  }
}

// Connection State UI Helper
function setConnectionState(connected, text) {
  state.isConnected = connected;
  el.connStatusText.textContent = text;
  if (connected) {
    el.connIndicator.className = 'status-indicator connected';
  } else if (state.isDemoMode) {
    el.connIndicator.className = 'status-indicator warning';
    el.connStatusText.textContent = 'Demo Mode (Simulated)';
  } else {
    el.connIndicator.className = 'status-indicator error';
  }
}

// Polling Controller
function restartPolling() {
  if (state.timerId) clearInterval(state.timerId);

  const loop = () => {
    if (state.isDemoMode) {
      generateDemoData();
    } else {
      fetchEsp32Data();
    }
  };

  loop();
  state.timerId = setInterval(loop, state.pollInterval);
}

// MQ-135 Warmup Countdown Controller
function startWarmupCountdown() {
  if (state.warmupTimerId) clearInterval(state.warmupTimerId);

  state.warmupTimerId = setInterval(() => {
    if (state.warmupRemaining > 0) {
      state.warmupRemaining--;
      el.warmupCountdown.textContent = `${state.warmupRemaining}s`;
    } else {
      clearInterval(state.warmupTimerId);
      el.warmupBanner.className = 'alert-banner banner-normal';
      el.warmupBanner.innerHTML = `
        <i class="fa-solid fa-circle-check banner-icon"></i>
        <div class="banner-content">
          <strong>MQ-135 Sensor Warm-Up Complete:</strong> Internal tin-dioxide (SnO2) heating element is fully stabilized at ~200°C.
        </div>
      `;
      setTimeout(() => {
        el.warmupBanner.style.opacity = '0.4';
      }, 5000);
    }
  }, 1000);
}

// Export Captured Telemetry to CSV File
function exportToCsv() {
  if (state.history.length === 0) {
    alert('No telemetry data collected yet to export!');
    return;
  }

  const headers = ['Timestamp', 'MQ135_ADC', 'Estimated_PPM', 'Voltage_V', 'Temp_C', 'Humidity_Pct', 'State', 'Green_LED', 'Red_LED', 'Buzzer'];
  const rows = state.history.map(h => [
    h.timestamp,
    h.gasAdc,
    h.gasPpm,
    h.voltage,
    h.tempC,
    h.humidity,
    h.status,
    h.greenLed,
    h.redLed,
    h.buzzer
  ]);

  const csvContent = [headers.join(','), ...rows.map(r => r.join(','))].join('\n');
  const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.setAttribute('href', url);
  link.setAttribute('download', `ESP32_AirQuality_Telemetry_${new Date().toISOString().slice(0, 19)}.csv`);
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
}

// Event Listeners Setup
function setupEventListeners() {
  // 0. Multi-Device Selector Change
  if (el.deviceSelector) {
    el.deviceSelector.addEventListener('change', (e) => {
      state.selectedDevice = e.target.value;
      fetchEsp32Data();
    });
  }

  // 1. Connect Button
  el.btnConnect.addEventListener('click', () => {
    const target = el.espIpInput.value.trim();
    if (target) {
      // Check if user entered an IP on HTTPS page
      if (window.location.protocol === 'https:' && !target.startsWith('https://')) {
        const rawHost = target.replace(/^https?:\/\//, '').replace(/\/.*$/, '');
        const isIp = /^(?:\d{1,3}\.){3}\d{1,3}(?::\d+)?$/.test(rawHost);
        if (isIp) {
          const openDirectly = confirm(
            `Browser HTTPS Security restricts fetching local ESP32 IP (${rawHost}) directly inside an HTTPS website.\n\n` +
            `Click OK to open your local ESP32 web interface directly at http://${rawHost}/ in a new tab!`
          );
          if (openDirectly) {
            window.open(`http://${rawHost}/`, '_blank');
            return;
          }
        }
      }
    }
    state.isDemoMode = false;
    el.demoBtnText.textContent = 'Live ESP32';
    el.btnToggleDemo.classList.add('btn-primary');
    el.btnToggleDemo.classList.remove('btn-secondary');
    restartPolling();
  });

  // 2. Demo Mode Toggle
  el.btnToggleDemo.addEventListener('click', () => {
    state.isDemoMode = !state.isDemoMode;
    if (state.isDemoMode) {
      el.demoBtnText.textContent = 'Demo Mode';
      el.btnToggleDemo.classList.remove('btn-primary');
      el.btnToggleDemo.classList.add('btn-secondary');
      setConnectionState(false, 'Demo Mode (Simulated)');
    } else {
      el.demoBtnText.textContent = 'Live ESP32';
      el.btnToggleDemo.classList.add('btn-primary');
      el.btnToggleDemo.classList.remove('btn-secondary');
      fetchEsp32Data();
    }
    restartPolling();
  });

  // 3. Audio Alarm Toggle
  el.btnToggleAudio.addEventListener('click', () => {
    state.audioMuted = !state.audioMuted;
    if (state.audioMuted) {
      el.audioIcon.className = 'fa-solid fa-volume-xmark';
      el.btnToggleAudio.style.color = '#ef4444';
    } else {
      el.audioIcon.className = 'fa-solid fa-volume-high';
      el.btnToggleAudio.style.color = '';
      playAlarmBeep(); // test beep
    }
  });

  // 4. Pinout Modal
  el.btnOpenPinout.addEventListener('click', () => {
    el.pinoutModal.classList.add('active');
  });
  el.btnCloseModal.addEventListener('click', () => {
    el.pinoutModal.classList.remove('active');
  });
  el.pinoutModal.addEventListener('click', (e) => {
    if (e.target === el.pinoutModal) el.pinoutModal.classList.remove('active');
  });

  // 5. Threshold Sliders
  el.warnThresholdSlider.addEventListener('input', (e) => {
    const val = parseInt(e.target.value);
    state.thresholds.warn = val;
    el.warnThresholdDisplay.textContent = val;
    el.legendWarnVal.textContent = val;
  });

  el.alertThresholdSlider.addEventListener('input', (e) => {
    const val = parseInt(e.target.value);
    state.thresholds.alert = val;
    el.alertThresholdDisplay.textContent = val;
    el.legendAlertVal.textContent = val;
  });

  el.dividerRatioInput.addEventListener('input', (e) => {
    const val = parseFloat(e.target.value);
    state.thresholds.voltageDividerRatio = val;
    el.dividerRatioDisplay.textContent = `${val.toFixed(2)}x`;
  });

  el.pollIntervalSelect.addEventListener('change', (e) => {
    state.pollInterval = parseInt(e.target.value);
    restartPolling();
  });

  // 6. Test Alarm Button (Simulates Critical Condition)
  el.btnTestAlarm.addEventListener('click', () => {
    renderTelemetry({
      gasAdc: 2850,
      gasPpm: 1200,
      gasVolt: 2.3,
      tempC: 28.5,
      humidity: 62,
      ...calculateIndices(28.5, 62)
    });
  });

  // 7. Save Thresholds to ESP32
  el.btnSaveThresholds.addEventListener('click', async () => {
    let targetUrl = el.espIpInput.value.trim() || state.endpoint;
    if (!targetUrl.startsWith('http')) targetUrl = 'http://' + targetUrl;

    if (!state.isDemoMode) {
      try {
        await fetch(`${targetUrl}/api/thresholds?warn=${state.thresholds.warn}&alert=${state.thresholds.alert}`, {
          method: 'POST'
        });
        alert('Thresholds saved successfully to ESP32!');
      } catch (e) {
        alert('Could not reach ESP32. Saved locally in web app.');
      }
    } else {
      alert(`Applied locally: Warning = ${state.thresholds.warn}, Alert = ${state.thresholds.alert}`);
    }
  });

  // 8. Log Actions
  el.btnClearLog.addEventListener('click', () => {
    el.telemetryTableBody.innerHTML = '';
    state.history = [];
  });
  el.btnExportCsv.addEventListener('click', exportToCsv);
}

// Application Initialization
window.addEventListener('DOMContentLoaded', () => {
  initCharts();
  setupEventListeners();
  startWarmupCountdown();

  // If accessed over HTTP/HTTPS (e.g. Render cloud or local node server), default to Live mode
  if (window.location.protocol.startsWith('http')) {
    state.isDemoMode = false;
    el.demoBtnText.textContent = 'Live Cloud';
    el.btnToggleDemo.classList.add('btn-primary');
    el.btnToggleDemo.classList.remove('btn-secondary');
    setConnectionState(true, 'Connecting to Cloud...');
  }

  // Auto-connect to Socket.io if running on Node.js / Cloud Server
  if (typeof io !== 'undefined') {
    try {
      const socket = io();
      socket.on('connect', () => {
        state.isDemoMode = false;
        el.demoBtnText.textContent = 'Live Cloud';
        el.btnToggleDemo.classList.add('btn-primary');
        el.btnToggleDemo.classList.remove('btn-secondary');
        setConnectionState(true, 'Cloud Live Connected');
        refreshDeviceList();
        fetchEsp32Data(); // Grab immediate snapshot
      });

      socket.on('telemetry_update', (d) => {
        state.isDemoMode = false;
        // If a specific device is selected by the user, ignore other devices
        if (state.selectedDevice && d.deviceId && d.deviceId !== state.selectedDevice) {
          return;
        }

        const gasProps = convertGas(d.adc, state.thresholds.voltageDividerRatio);
        const tempVal = (d.temp !== undefined && d.temp !== null) ? Number(d.temp) : 0.0;
        const humVal = (d.hum !== undefined && d.hum !== null) ? Number(d.hum) : 0.0;
        const indices = calculateIndices(tempVal, humVal);
        renderTelemetry({
          gasAdc: d.adc,
          gasPpm: d.ppm || gasProps.ppm,
          gasVolt: d.voltage || gasProps.measuredV,
          tempC: tempVal,
          humidity: humVal,
          greenLed: Boolean(d.green),
          redLed: Boolean(d.red),
          buzzer: Boolean(d.buzz),
          ...indices
        });
      });

      socket.on('disconnect', () => {
        setConnectionState(false, 'Cloud Disconnected');
      });
    } catch (e) {
      console.warn('Socket.io init notice:', e);
    }
  }

  restartPolling();
});

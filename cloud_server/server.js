const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const cors = require('cors');
const path = require('path');

const app = express();
const server = http.createServer(app);
const io = new Server(server, {
  cors: { origin: '*' }
});

const PORT = process.env.PORT || 3000;

// Resolve web_dashboard folder whether started from root or cloud_server
const fs = require('fs');
let publicDir = path.join(__dirname, '../web_dashboard');
if (!fs.existsSync(publicDir)) {
  publicDir = path.join(__dirname, 'web_dashboard');
}

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// Serve frontend dashboard files statically
app.use(express.static(publicDir));

// In-memory state & multi-device buffer
let devices = {};
let currentTelemetry = {
  deviceId: 'ESP32-Primary',
  adc: 2400,
  ppm: 420,
  voltage: 1.93,
  temp: 28.5,
  hum: 55,
  status: 'NORMAL',
  green: 1,
  red: 0,
  buzz: 0,
  warnThresh: 2700,
  alertThresh: 3200,
  updatedAt: new Date().toISOString()
};
devices['ESP32-Primary'] = currentTelemetry;

const telemetryHistory = [];
const MAX_HISTORY = 100;

// REST API 1: Health check
app.get('/api/health', (req, res) => {
  res.json({ status: 'online', clientsConnected: io.engine.clientsCount, devicesCount: Object.keys(devices).length, time: new Date() });
});

// REST API 2: List all connected devices
app.get('/api/devices', (req, res) => {
  const deviceList = Object.keys(devices).map(id => ({
    id,
    lastSeen: devices[id].updatedAt,
    status: devices[id].status
  }));
  res.json(deviceList);
});

// REST API 3: Get current telemetry snapshot (optionally by device)
app.get('/api/data', (req, res) => {
  const targetDevice = req.query.device;
  if (targetDevice && devices[targetDevice]) {
    return res.json(devices[targetDevice]);
  }
  res.json(currentTelemetry);
});

// REST API 4: Get historical log
app.get('/api/history', (req, res) => {
  const targetDevice = req.query.device;
  if (targetDevice) {
    return res.json(telemetryHistory.filter(h => h.deviceId === targetDevice));
  }
  res.json(telemetryHistory);
});

// REST API 5: ESP32 pushes new analog telemetry here
app.post('/api/telemetry', (req, res) => {
  const { adc, temp, hum, ppm, green, red, buzz } = req.body;
  const devId = (req.body.deviceId || req.body.device || req.query.device || 'ESP32-Primary').trim();

  if (adc === undefined) {
    return res.status(400).json({ error: 'Missing adc value in payload' });
  }

  const existingDev = devices[devId] || currentTelemetry;
  const warnT = existingDev.warnThresh || 2700;
  const alertT = existingDev.alertThresh || 3200;

  // Derive status based on thresholds
  let status = 'NORMAL';
  if (adc >= alertT) {
    status = 'ALERT';
  } else if (adc >= warnT) {
    status = 'WARNING';
  }

  const measuredV = (adc / 4095.0) * 3.3;

  const deviceTelemetry = {
    deviceId: devId,
    adc: Number(adc),
    ppm: ppm ? Number(ppm) : Math.round(400 + Math.pow(adc / 600.0, 2.5) * 85),
    voltage: Number(measuredV.toFixed(2)),
    temp: temp !== undefined ? Number(temp) : (existingDev.temp || 0),
    hum: hum !== undefined ? Number(hum) : (existingDev.hum || 0),
    status,
    green: green !== undefined ? Number(green) : (status === 'NORMAL' ? 1 : 0),
    red: red !== undefined ? Number(red) : (status !== 'NORMAL' ? 1 : 0),
    buzz: buzz !== undefined ? Number(buzz) : (status === 'ALERT' ? 1 : 0),
    warnThresh: warnT,
    alertThresh: alertT,
    updatedAt: new Date().toISOString()
  };

  devices[devId] = deviceTelemetry;
  currentTelemetry = deviceTelemetry;

  // Add to history
  telemetryHistory.unshift(deviceTelemetry);
  if (telemetryHistory.length > MAX_HISTORY) telemetryHistory.pop();

  // Real-time broadcast to all browser clients
  io.emit('telemetry_update', deviceTelemetry);

  console.log(`[ESP32: ${devId}] ADC: ${deviceTelemetry.adc} | Temp: ${deviceTelemetry.temp}C | Hum: ${deviceTelemetry.hum}% | Status: ${deviceTelemetry.status}`);

  res.status(200).json({ success: true, received: deviceTelemetry });
});

// REST API 5: Update thresholds from web UI
app.post('/api/thresholds', (req, res) => {
  const { warn, alert } = req.query;
  if (warn) currentTelemetry.warnThresh = Number(warn);
  if (alert) currentTelemetry.alertThresh = Number(alert);
  io.emit('thresholds_updated', { warn: currentTelemetry.warnThresh, alert: currentTelemetry.alertThresh });
  res.json({ success: true, thresholds: { warn: currentTelemetry.warnThresh, alert: currentTelemetry.alertThresh } });
});

// WebSocket Connection Handler
io.on('connection', (socket) => {
  console.log(`[Socket.io] Client connected: ${socket.id}`);
  // Send current state immediately on connect
  socket.emit('telemetry_update', currentTelemetry);

  socket.on('disconnect', () => {
    console.log(`[Socket.io] Client disconnected: ${socket.id}`);
  });
});

// Start Server
server.listen(PORT, () => {
  console.log(`=======================================================`);
  console.log(` AirSense ESP32 Cloud Hub Server running on port ${PORT}`);
  console.log(` Web Dashboard available at: http://localhost:${PORT}`);
  console.log(` ESP32 Telemetry POST endpoint: http://localhost:${PORT}/api/telemetry`);
  console.log(`=======================================================`);
});

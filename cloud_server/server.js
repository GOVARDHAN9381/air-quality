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

// In-memory state & history buffer
let currentTelemetry = {
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

const telemetryHistory = [];
const MAX_HISTORY = 100;

// REST API 1: Health check
app.get('/api/health', (req, res) => {
  res.json({ status: 'online', clientsConnected: io.engine.clientsCount, time: new Date() });
});

// REST API 2: Get current telemetry snapshot
app.get('/api/data', (req, res) => {
  res.json(currentTelemetry);
});

// REST API 3: Get historical log
app.get('/api/history', (req, res) => {
  res.json(telemetryHistory);
});

// REST API 4: ESP32 pushes new analog telemetry here
app.post('/api/telemetry', (req, res) => {
  const { adc, temp, hum, ppm, green, red, buzz } = req.body;

  if (adc === undefined) {
    return res.status(400).json({ error: 'Missing adc value in payload' });
  }

  // Derive status based on thresholds
  let status = 'NORMAL';
  if (adc >= currentTelemetry.alertThresh) {
    status = 'ALERT';
  } else if (adc >= currentTelemetry.warnThresh) {
    status = 'WARNING';
  }

  const measuredV = (adc / 4095.0) * 3.3;

  currentTelemetry = {
    ...currentTelemetry,
    adc: Number(adc),
    ppm: ppm ? Number(ppm) : Math.round(400 + Math.pow(adc / 600.0, 2.5) * 85),
    voltage: Number(measuredV.toFixed(2)),
    temp: temp !== undefined ? Number(temp) : currentTelemetry.temp,
    hum: hum !== undefined ? Number(hum) : currentTelemetry.hum,
    status,
    green: green !== undefined ? Number(green) : (status === 'NORMAL' ? 1 : 0),
    red: red !== undefined ? Number(red) : (status !== 'NORMAL' ? 1 : 0),
    buzz: buzz !== undefined ? Number(buzz) : (status === 'ALERT' ? 1 : 0),
    updatedAt: new Date().toISOString()
  };

  // Add to history
  telemetryHistory.unshift(currentTelemetry);
  if (telemetryHistory.length > MAX_HISTORY) telemetryHistory.pop();

  // Real-time broadcast to all browser clients
  io.emit('telemetry_update', currentTelemetry);

  console.log(`[ESP32 Telemetry] ADC: ${currentTelemetry.adc} | Temp: ${currentTelemetry.temp}C | Hum: ${currentTelemetry.hum}% | Status: ${currentTelemetry.status}`);

  res.status(200).json({ success: true, received: currentTelemetry });
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

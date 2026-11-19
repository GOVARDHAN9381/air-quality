// Zero-Dependency HTTP & Telemetry Server for ESP32 Air Quality Monitor
// Runs with vanilla Node.js - No npm install required!

const http = require('http');
const fs = require('fs');
const path = require('path');
const url = require('url');

const PORT = process.env.PORT || 3000;
const PUBLIC_DIR = path.join(__dirname, '../web_dashboard');

// In-memory telemetry state
let currentTelemetry = {
  adc: 1250,
  ppm: 415,
  voltage: 1.01,
  temp: 26.5,
  hum: 54,
  status: 'NORMAL',
  green: 1,
  red: 0,
  buzz: 0,
  warnThresh: 1200,
  alertThresh: 2000,
  updatedAt: new Date().toISOString()
};

const history = [];

// MIME types lookup
const MIME_TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon'
};

const server = http.createServer((req, res) => {
  // CORS Headers
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

  if (req.method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
    return;
  }

  const parsedUrl = url.parse(req.url, true);
  const pathname = parsedUrl.pathname;

  // 1. API: GET /api/data (Telemetry for web dashboard)
  if (req.method === 'GET' && pathname === '/api/data') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(currentTelemetry));
    return;
  }

  // 2. API: POST /api/telemetry (ESP32 pushes analog sensor data)
  if (req.method === 'POST' && pathname === '/api/telemetry') {
    let body = '';
    req.on('data', chunk => { body += chunk; });
    req.on('end', () => {
      try {
        const payload = JSON.parse(body);
        const adc = Number(payload.adc);

        let status = 'NORMAL';
        if (adc >= currentTelemetry.alertThresh) {
          status = 'ALERT';
        } else if (adc >= currentTelemetry.warnThresh) {
          status = 'WARNING';
        }

        const measuredV = (adc / 4095.0) * 3.3;

        currentTelemetry = {
          ...currentTelemetry,
          adc,
          ppm: payload.ppm ? Number(payload.ppm) : Math.round(400 + Math.pow(adc / 600.0, 2.5) * 85),
          voltage: Number(measuredV.toFixed(2)),
          temp: payload.temp !== undefined ? Number(payload.temp) : currentTelemetry.temp,
          hum: payload.hum !== undefined ? Number(payload.hum) : currentTelemetry.hum,
          status,
          green: payload.green !== undefined ? Number(payload.green) : (status === 'NORMAL' ? 1 : 0),
          red: payload.red !== undefined ? Number(payload.red) : (status !== 'NORMAL' ? 1 : 0),
          buzz: payload.buzz !== undefined ? Number(payload.buzz) : (status === 'ALERT' ? 1 : 0),
          updatedAt: new Date().toISOString()
        };

        history.unshift(currentTelemetry);
        if (history.length > 50) history.pop();

        console.log(`[ESP32 -> Web] ADC: ${currentTelemetry.adc} | Temp: ${currentTelemetry.temp}C | Hum: ${currentTelemetry.hum}% | State: ${currentTelemetry.status}`);

        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ success: true, telemetry: currentTelemetry }));
      } catch (err) {
        res.writeHead(400, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Invalid JSON payload' }));
      }
    });
    return;
  }

  // 3. API: POST /api/thresholds
  if (req.method === 'POST' && pathname === '/api/thresholds') {
    if (parsedUrl.query.warn) currentTelemetry.warnThresh = Number(parsedUrl.query.warn);
    if (parsedUrl.query.alert) currentTelemetry.alertThresh = Number(parsedUrl.query.alert);
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ success: true, thresholds: { warn: currentTelemetry.warnThresh, alert: currentTelemetry.alertThresh } }));
    return;
  }

  // 4. Static File Server for Web Dashboard
  let filePath = path.join(PUBLIC_DIR, pathname === '/' ? 'index.html' : pathname);
  const ext = path.extname(filePath).toLowerCase();
  const contentType = MIME_TYPES[ext] || 'application/octet-stream';

  fs.readFile(filePath, (err, content) => {
    if (err) {
      if (err.code === 'ENOENT') {
        // Fallback to index.html for SPA routing
        fs.readFile(path.join(PUBLIC_DIR, 'index.html'), (err2, fallbackContent) => {
          if (err2) {
            res.writeHead(404, { 'Content-Type': 'text/plain' });
            res.end('404 Not Found');
          } else {
            res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
            res.end(fallbackContent);
          }
        });
      } else {
        res.writeHead(500);
        res.end(`Server Error: ${err.code}`);
      }
    } else {
      res.writeHead(200, { 'Content-Type': contentType });
      res.end(content);
    }
  });
});

server.listen(PORT, () => {
  console.log('================================================================');
  console.log(` AirSense Telemetry Server listening on: http://localhost:${PORT}`);
  console.log(`   - Open Web Dashboard: http://localhost:${PORT}`);
  console.log(`   - Telemetry Stream:   http://localhost:${PORT}/api/data`);
  console.log(`   - ESP32 POST Endpoint: http://localhost:${PORT}/api/telemetry`);
  console.log('================================================================');
});

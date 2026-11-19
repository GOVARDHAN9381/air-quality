/*
 * =========================================================================================
 * Project: ESP32 Standalone Web Server - Real-Time Air Quality Monitoring System
 * Course: Microprocessors & Microcontrollers (MPMC T-3)
 * Target Hardware: ESP32 DevKit V1 (30-pin)
 * 
 * Hardware Connections (Streamlined Breadboard Setup - No OLED/LEDs):
 *   - DHT11 Sensor:      GND -> GND, VCC -> 3.3V, DATA -> GPIO 12
 *   - MQ-135 Gas Sensor: GND -> GND, VCC -> 5V,   A0 -> GPIO 34 (via voltage divider)
 *   - Active Buzzer:     Negative (-) -> GND,     Positive (+) -> GPIO 21
 * 
 * Note: OLED display and physical LEDs are omitted due to breadboard space limits.
 * The ESP32 hosts its own Wi-Fi Web Dashboard which displays live gauges,
 * PPM readings, temperature, humidity, and visual warning banners!
 * =========================================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// ================= PINS CONFIGURATION =================
#define PIN_MQ135         34  // Analog Input ADC1_CH6 (via voltage divider)
#define PIN_DHT           12  // Digital 1-wire pin for DHT11
#define PIN_BUZZER        21  // Active Buzzer Positive

#define DHTTYPE           DHT11

// ================= THRESHOLD DEFAULTS =================
int thresholdWarning = 1300;   // Moderate gas level -> Web Warning State
int thresholdAlert   = 2100;   // Critical gas / smoke -> Web Alert + Buzzer Siren

// ================= NETWORK CREDENTIALS =================
// 1. Station Mode (Connects to existing router or mobile hotspot)
const char* wifi_ssid     = "YOUR_WIFI_NAME";     // <-- Change to your Wi-Fi name
const char* wifi_password = "YOUR_WIFI_PASSWORD"; // <-- Change to your Wi-Fi password

// 2. SoftAP Fallback (ESP32 creates its own hotspot for college presentations)
const char* ap_ssid       = "AirSense-ESP32-Hotspot";
const char* ap_password   = "12345678";

// ================= OBJECTS & GLOBALS =================
WebServer server(80);
DHT dht(PIN_DHT, DHTTYPE);

// Sensor readings
int rawAdc = 0;
float voltage = 0.0;
int estimatedPpm = 400;
float temperatureC = 0.0;
float humidityPct = 0.0;
String systemStatus = "NORMAL"; // NORMAL | WARNING | ALERT

// Non-blocking timer tracking
unsigned long lastSensorReadTime = 0;
const unsigned long SENSOR_INTERVAL = 2000; // 2 seconds between DHT11 reads
unsigned long buzzerBlinkTime = 0;
bool buzzerToggle = false;

// ================= MQ-135 OVERSAMPLED READING =================
int readMQ135Filtered() {
  long sum = 0;
  const int NUM_SAMPLES = 16;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += analogRead(PIN_MQ135);
    delayMicroseconds(500);
  }
  return sum / NUM_SAMPLES;
}

// Approximate PPM equation for display
int calculateEstimatedPpm(int adc) {
  if (adc < 600) return 400;
  float ratio = (float)adc / 600.0;
  int ppm = 400 + (int)(pow(ratio, 2.5) * 85.0);
  if (ppm > 9999) ppm = 9999;
  return ppm;
}

// ================= HARDWARE UPDATE LOGIC =================
void updateBuzzerAndStatus() {
  if (rawAdc >= thresholdAlert) {
    systemStatus = "ALERT";
    // Intermittent alarm pulsing
    if (millis() - buzzerBlinkTime > 300) {
      buzzerBlinkTime = millis();
      buzzerToggle = !buzzerToggle;
      digitalWrite(PIN_BUZZER, buzzerToggle ? HIGH : LOW);
    }
  } else if (rawAdc >= thresholdWarning) {
    systemStatus = "WARNING";
    digitalWrite(PIN_BUZZER, LOW);
  } else {
    systemStatus = "NORMAL";
    digitalWrite(PIN_BUZZER, LOW);
  }
}

// ================= WEB SERVER API HANDLERS =================

// Handler 1: /api/data (Returns real-time JSON for web dashboard)
void handleApiData() {
  String json = "{";
  json += "\"adc\":" + String(rawAdc) + ",";
  json += "\"ppm\":" + String(estimatedPpm) + ",";
  json += "\"voltage\":" + String(voltage, 2) + ",";
  json += "\"temp\":" + String(isnan(temperatureC) ? 0.0 : temperatureC, 1) + ",";
  json += "\"hum\":" + String(isnan(humidityPct) ? 0.0 : humidityPct, 1) + ",";
  json += "\"status\":\"" + systemStatus + "\",";
  json += "\"warnThresh\":" + String(thresholdWarning) + ",";
  json += "\"alertThresh\":" + String(thresholdAlert) + ",";
  json += "\"green\":" + String(systemStatus == "NORMAL" ? 1 : 0) + ",";
  json += "\"red\":" + String(systemStatus != "NORMAL" ? 1 : 0) + ",";
  json += "\"buzz\":" + String(systemStatus == "ALERT" ? 1 : 0) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\"";
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// Handler 2: /api/thresholds (Allows dynamic threshold adjustment from web UI)
void handleApiThresholds() {
  if (server.hasArg("warn")) {
    thresholdWarning = server.arg("warn").toInt();
  }
  if (server.hasArg("alert")) {
    thresholdAlert = server.arg("alert").toInt();
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "OK");
}

// Handler 3: Embedded Web Dashboard HTML (Root Page "/")
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>AirSense ESP32 Live Monitor</title>
  <style>
    body { font-family: system-ui, -apple-system, sans-serif; background: #0a0e17; color: #f9fafb; margin: 0; padding: 1.5rem; text-align: center; }
    .card { background: #111827; border: 1px solid rgba(255,255,255,0.1); border-radius: 16px; max-width: 480px; margin: auto; padding: 1.5rem; box-shadow: 0 10px 25px rgba(0,0,0,0.6); }
    h2 { margin-top: 0; color: #6366f1; font-size: 1.4rem; }
    .status-badge { display: inline-block; padding: 0.4rem 1rem; border-radius: 99px; font-weight: 700; font-size: 0.9rem; margin: 0.5rem 0 1rem 0; letter-spacing: 0.05em; }
    .status-NORMAL { background: rgba(16,185,129,0.2); color: #34d399; border: 1px solid rgba(16,185,129,0.4); }
    .status-WARNING { background: rgba(245,158,11,0.2); color: #fbbf24; border: 1px solid rgba(245,158,11,0.4); }
    .status-ALERT { background: rgba(239,68,68,0.25); color: #f87171; border: 1px solid rgba(239,68,68,0.5); animation: blink 0.8s infinite; }
    .val-box { font-size: 3rem; font-weight: 800; font-family: monospace; color: #10b981; margin: 0.2rem 0; }
    .metrics { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 0.5rem; margin: 1.2rem 0; background: rgba(0,0,0,0.3); padding: 1rem; border-radius: 12px; }
    .m-item span { font-size: 0.75rem; color: #9ca3af; text-transform: uppercase; }
    .m-item strong { display: block; font-size: 1.3rem; margin-top: 0.2rem; font-family: monospace; }
    .hardware-info { font-size: 0.78rem; color: #9ca3af; border-top: 1px solid rgba(255,255,255,0.08); padding-top: 1rem; text-align: left; }
    .hw-row { display: flex; justify-content: space-between; margin-bottom: 0.35rem; }
    .tag { font-family: monospace; color: #a5b4fc; }
    @keyframes blink { 50% { opacity: 0.3; } }
  </style>
</head>
<body>
  <div class="card">
    <h2>AirSense ESP32 Live Hub</h2>
    <div id="statusBadge" class="status-badge status-NORMAL">AIR QUALITY: NORMAL</div>
    
    <div class="val-box" id="gasAdc">---</div>
    <div style="color: #9ca3af; font-size: 0.85rem;">MQ-135 ADC (GPIO 34) &bull; <span id="gasVolt">0.00</span> V</div>

    <div class="metrics">
      <div class="m-item">
        <span style="color:#06b6d4;">Temp</span>
        <strong id="temp">-- &deg;C</strong>
      </div>
      <div class="m-item">
        <span style="color:#3b82f6;">Humidity</span>
        <strong id="hum">-- %</strong>
      </div>
      <div class="m-item">
        <span style="color:#a78bfa;">PPM eq.</span>
        <strong id="ppm">--</strong>
      </div>
    </div>

    <div class="hardware-info">
      <div class="hw-row"><span>MQ-135 Gas Sensor:</span> <span class="tag">GPIO 34 (Analog In)</span></div>
      <div class="hw-row"><span>DHT11 Temp/Hum:</span> <span class="tag">GPIO 12 (1-Wire)</span></div>
      <div class="hw-row"><span>Buzzer Siren:</span> <span class="tag" id="buzzStatus">GPIO 21 (SILENT)</span></div>
      <div class="hw-row"><span>Visual Dashboard:</span> <span class="tag" style="color:#34d399;">Web Active (Replaces OLED)</span></div>
    </div>
  </div>

  <script>
    async function updateData() {
      try {
        const res = await fetch('/api/data');
        const d = await res.json();
        document.getElementById('gasAdc').textContent = d.adc;
        document.getElementById('gasVolt').textContent = d.voltage;
        document.getElementById('temp').innerHTML = d.temp + ' &deg;C';
        document.getElementById('hum').textContent = d.hum + ' %';
        document.getElementById('ppm').textContent = d.ppm;

        const badge = document.getElementById('statusBadge');
        badge.className = 'status-badge status-' + d.status;
        badge.textContent = 'AIR QUALITY: ' + d.status;

        const buzz = document.getElementById('buzzStatus');
        if (d.buzz) {
          buzz.textContent = 'GPIO 21 (SOUNDING ALARM!)';
          buzz.style.color = '#ef4444';
        } else {
          buzz.textContent = 'GPIO 21 (SILENT)';
          buzz.style.color = '#a5b4fc';
        }
      } catch (e) {
        console.warn('Poll failed');
      }
    }
    setInterval(updateData, 1500);
    updateData();
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- ESP32 Air Quality Monitoring System Starting ---");

  // Configure Buzzer Pin
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  // Initialize DHT11
  dht.begin();

  // Configure ADC resolution & attenuation for MQ-135 on GPIO 34
  analogReadResolution(12); // 12-bit: 0 to 4095
  analogSetAttenuation(ADC_11db); // Full 0 - 3.3V input range

  // Initialize Wi-Fi: Station Mode + SoftAP Dual Mode
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  Serial.print("Connecting to Wi-Fi SSID: ");
  Serial.println(wifi_ssid);

  // Wait up to 6 seconds for router connection
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 12) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi Connected successfully!");
    Serial.print("Station IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nRouter Wi-Fi not available. Running in Standalone Hotspot mode.");
  }

  // Start SoftAP Hotspot as guaranteed fallback for viva/demo
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("SoftAP Created! SSID: ");
  Serial.print(ap_ssid);
  Serial.print(" | IP: ");
  Serial.println(WiFi.softAPIP());

  // Set up Web Server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleApiData);
  server.on("/api/thresholds", HTTP_POST, handleApiThresholds);

  server.begin();
  Serial.println("HTTP Web Server Started on Port 80.");
}

// ================= MAIN LOOP =================
void loop() {
  // 1. Process incoming HTTP requests from browsers/dashboard
  server.handleClient();

  // 2. Read Sensors periodically (every 2 seconds)
  unsigned long now = millis();
  if (now - lastSensorReadTime >= SENSOR_INTERVAL) {
    lastSensorReadTime = now;

    // Read MQ-135 with filter
    rawAdc = readMQ135Filtered();
    voltage = (rawAdc / 4095.0) * 3.3;
    estimatedPpm = calculateEstimatedPpm(rawAdc);

    // Read DHT11
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
      humidityPct = h;
      temperatureC = t;
    }

    // Print to Serial Monitor for debugging
    Serial.printf("ADC: %d | Volt: %.2fV | Temp: %.1fC | Hum: %.1f%% | Status: %s\n",
                  rawAdc, voltage, temperatureC, humidityPct, systemStatus.c_str());
  }

  // 3. Update Buzzer Actuator
  updateBuzzerAndStatus();
}

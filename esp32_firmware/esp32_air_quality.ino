/*
 * =========================================================================================
 * Project: ESP32-Based Real-Time Indoor Air Quality Monitoring & Intelligent Warning System
 * Course: Microprocessors & Microcontrollers (MPMC T-3)
 * Target Hardware: ESP32 DevKit V1 (30-pin)
 * 
 * Hardware Connections:
 *   - DHT11 Sensor:        GND -> GND, VCC -> 3.3V, DATA (D) -> GPIO 12
 *   - MQ-135 Gas Sensor:   GND -> GND, VCC -> 5V,   A0 -> GPIO 34 (via 10k/20k voltage divider)
 *   - Active Buzzer:       Negative (-) -> GND,     Positive (P) -> GPIO 21
 *   - Green LED (Safe):    Cathode (-) -> GND,      Anode (+) -> GPIO 18 (via 220-330 ohm resistor)
 *   - Red LED (Warning):   Cathode (-) -> GND,      Anode (+) -> GPIO 19 (via 220-330 ohm resistor)
 *   - 0.96" OLED Display:  GND -> GND, VCC -> 3.3V, SDA -> GPIO 22, SCL -> GPIO 23
 * 
 * Required Arduino Libraries:
 *   1. "DHT sensor library" by Adafruit
 *   2. "Adafruit SSD1306" by Adafruit
 *   3. "Adafruit GFX Library" by Adafruit
 *   4. Built-in: <WiFi.h>, <WebServer.h>, <Wire.h>
 * =========================================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ================= PINS CONFIGURATION =================
#define PIN_MQ135         34  // Analog Input ADC1_CH6 (via voltage divider)
#define PIN_DHT           12  // Digital 1-wire pin for DHT11
#define PIN_BUZZER        21  // Active Buzzer Positive (P)
#define PIN_LED_GREEN     18  // Normal Condition Green LED
#define PIN_LED_RED       19  // Warning / Alert Red LED
#define PIN_OLED_SDA      22  // OLED I2C SDA wire
#define PIN_OLED_SCL      23  // OLED I2C SCL wire

#define DHTTYPE           DHT11
#define SCREEN_WIDTH      128
#define SCREEN_HEIGHT     64
#define OLED_RESET        -1
#define SCREEN_I2C_ADDR   0x3C // Default SSD1306 address

// ================= THRESHOLD DEFAULTS =================
// Calibrated for your room's clean air baseline (current baseline: ~2300-2500)
int thresholdWarning = 2700;   // Moderate gas level -> Red LED ON, Web Warning
int thresholdAlert   = 3200;   // Critical gas / smoke -> Red LED Flashing + Buzzer Siren

// ================= NETWORK CREDENTIALS =================
// 1. Station Mode (Connects to existing router or mobile hotspot)
const char* wifi_ssid     = "YOUR_WIFI_NAME";     // <-- Change to your Wi-Fi name
const char* wifi_password = "YOUR_WIFI_PASSWORD"; // <-- Change to your Wi-Fi password

// 2. Cloud Website URL (Render Cloud Server Endpoint)
const char* cloudServerUrl = "https://air-quality-monitoring-75du.onrender.com/api/telemetry";

// 3. Device Identification (Each student/friend can set their own name!)
const char* deviceId      = "ESP32-Govardhan";    // <-- Change to your name (e.g. ESP32-Rahul, ESP32-Team1)

// 3. SoftAP Fallback (ESP32 creates its own hotspot for college presentations)
const char* ap_ssid       = "AirSense-ESP32-Hotspot";
const char* ap_password   = "12345678";

// ================= OBJECTS & GLOBALS =================
WebServer server(80);
DHT dht(PIN_DHT, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledAvailable = false;

// Sensor readings
int rawAdc = 0;
float voltage = 0.0;
int estimatedPpm = 400;
float temperatureC = 0.0;
float humidityPct = 0.0;
String systemStatus = "NORMAL"; // NORMAL | WARNING | ALERT

// Actuator states
bool stateGreenLed = true;
bool stateRedLed = false;
bool stateBuzzer = false;

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

// Push JSON telemetry to Render / Cloud Website (if URL is set)
void sendTelemetryToCloud() {
  if (strlen(cloudServerUrl) == 0 || WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure(); // Disable SSL certificate validation for Render HTTPS
  HTTPClient http;

  if (!http.begin(client, cloudServerUrl)) {
    Serial.println("[Cloud] Connection failed to initialize");
    return;
  }
  http.addHeader("Content-Type", "application/json");

  String jsonPayload = "{";
  jsonPayload += "\"deviceId\":\"" + String(deviceId) + "\",";
  jsonPayload += "\"adc\":" + String(rawAdc) + ",";
  jsonPayload += "\"ppm\":" + String(estimatedPpm) + ",";
  jsonPayload += "\"voltage\":" + String(voltage, 2) + ",";
  jsonPayload += "\"temp\":" + String(isnan(temperatureC) ? 0.0 : temperatureC, 1) + ",";
  jsonPayload += "\"hum\":" + String(isnan(humidityPct) ? 0.0 : humidityPct, 1) + ",";
  jsonPayload += "\"status\":\"" + systemStatus + "\",";
  jsonPayload += "\"green\":" + String(stateGreenLed ? 1 : 0) + ",";
  jsonPayload += "\"red\":" + String(stateRedLed ? 1 : 0) + ",";
  jsonPayload += "\"buzz\":" + String(stateBuzzer ? 1 : 0);
  jsonPayload += "}";

  int httpCode = http.POST(jsonPayload);
  if (httpCode > 0) {
    Serial.printf("[Cloud Upload OK: Code %d]\n", httpCode);
  } else {
    Serial.printf("[Cloud Upload Failed: %s]\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

// ================= HARDWARE UPDATE LOGIC =================
void updateActuatorsAndStatus() {
  if (rawAdc >= thresholdAlert) {
    systemStatus = "ALERT";
    stateGreenLed = false;
    stateRedLed = true;
    stateBuzzer = true;
  } else if (rawAdc >= thresholdWarning) {
    systemStatus = "WARNING";
    stateGreenLed = false;
    stateRedLed = true;
    stateBuzzer = false;
  } else {
    systemStatus = "NORMAL";
    stateGreenLed = true;
    stateRedLed = false;
    stateBuzzer = false;
  }

  // Update Green LED
  digitalWrite(PIN_LED_GREEN, stateGreenLed ? HIGH : LOW);

  // Red LED & Buzzer handling
  if (systemStatus == "ALERT") {
    // Intermittent alarm pulsing
    if (millis() - buzzerBlinkTime > 300) {
      buzzerBlinkTime = millis();
      buzzerToggle = !buzzerToggle;
      digitalWrite(PIN_LED_RED, buzzerToggle ? HIGH : LOW);
      digitalWrite(PIN_BUZZER, buzzerToggle ? HIGH : LOW);
    }
  } else if (systemStatus == "WARNING") {
    digitalWrite(PIN_LED_RED, HIGH);
    digitalWrite(PIN_BUZZER, LOW);
  } else {
    digitalWrite(PIN_LED_RED, LOW);
    digitalWrite(PIN_BUZZER, LOW);
  }
}

// ================= OLED DISPLAY UPDATE =================
void updateOLED() {
  if (!oledAvailable) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Line 1: Header / Title
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("AIR MONITOR [");
  display.print(systemStatus);
  display.print("]");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  // Line 2: Air Quality Metric
  display.setCursor(0, 14);
  display.print("Air ADC: ");
  display.print(rawAdc);
  display.print(" (");
  display.print(estimatedPpm);
  display.print("ppm)");

  // Line 3: Temperature & Humidity
  display.setCursor(0, 27);
  display.print("Temp: ");
  if (isnan(temperatureC)) {
    display.print("-- C");
  } else {
    display.print(temperatureC, 1);
    display.print((char)247); // Degree symbol
    display.print("C");
  }

  display.setCursor(0, 40);
  display.print("Humidity: ");
  if (isnan(humidityPct)) {
    display.print("-- %");
  } else {
    display.print(humidityPct, 1);
    display.print(" %");
  }

  // Line 4: System IP address (So you know what to type in browser!)
  display.drawLine(0, 52, 127, 52, SSD1306_WHITE);
  display.setCursor(0, 55);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("IP: ");
    display.print(WiFi.localIP().toString());
  } else {
    display.print("AP: 192.168.4.1");
  }

  display.display();
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
  json += "\"green\":" + String(stateGreenLed ? 1 : 0) + ",";
  json += "\"red\":" + String(stateRedLed ? 1 : 0) + ",";
  json += "\"buzz\":" + String(stateBuzzer ? 1 : 0) + ",";
  json += "\"oled\":" + String(oledAvailable ? 1 : 0) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\"";
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(200, "application/json", json);
}

// Handler: Preflight OPTIONS request for browser CORS
void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(204);
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
    .dot { width: 10px; height: 10px; border-radius: 50%; display: inline-block; margin-right: 5px; }
    .dot-grn { background: #10b981; box-shadow: 0 0 8px #10b981; }
    .dot-red { background: #ef4444; box-shadow: 0 0 8px #ef4444; }
    .dot-off { background: #4b5563; }
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
      <div class="hw-row"><span>Green LED (GPIO 18):</span> <span class="tag"><span class="dot dot-grn" id="grnDot"></span><span id="grnTxt">ON</span></span></div>
      <div class="hw-row"><span>Red LED (GPIO 19):</span> <span class="tag"><span class="dot dot-off" id="redDot"></span><span id="redTxt">OFF</span></span></div>
      <div class="hw-row"><span>Active Buzzer (GPIO 21):</span> <span class="tag" id="buzzStatus">SILENT</span></div>
      <div class="hw-row"><span>OLED (GPIO 22/23):</span> <span class="tag" id="oledStatus">DETECTED</span></div>
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

        document.getElementById('grnDot').className = d.green ? 'dot dot-grn' : 'dot dot-off';
        document.getElementById('grnTxt').textContent = d.green ? 'ON' : 'OFF';

        document.getElementById('redDot').className = d.red ? 'dot dot-red' : 'dot dot-off';
        document.getElementById('redTxt').textContent = d.red ? 'ON' : 'OFF';

        const buzz = document.getElementById('buzzStatus');
        if (d.buzz) {
          buzz.textContent = 'SOUNDING ALARM!';
          buzz.style.color = '#ef4444';
        } else {
          buzz.textContent = 'SILENT';
          buzz.style.color = '#a5b4fc';
        }

        document.getElementById('oledStatus').textContent = d.oled ? 'ACTIVE (I2C)' : 'WEB DASHBOARD';
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

  // Configure Actuator Pins
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  // Initial Safe States
  digitalWrite(PIN_LED_GREEN, HIGH);
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  // Initialize Custom I2C (SDA = GPIO 22, SCL = GPIO 23)
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);

  // Initialize OLED (Graceful fallback if OLED is not attached)
  if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_I2C_ADDR)) {
    oledAvailable = true;
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(10, 20);
    display.println("AirSense ESP32");
    display.setCursor(10, 35);
    display.println("System Ready...");
    display.display();
    delay(1000);
  } else {
    Serial.println("[Notice] OLED display not detected on GPIO 22/23. Web Dashboard active.");
  }

  // Initialize DHT11
  dht.begin();

  // Configure ADC resolution & attenuation for MQ-135 on GPIO 34
  analogReadResolution(12); // 12-bit: 0 to 4095
  analogSetAttenuation(ADC_11db); // Full 0 - 3.3V input range

  // Initialize Wi-Fi: Station Mode + SoftAP Dual Mode
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifi_ssid, wifi_password);
  Serial.print("Connecting to Hotspot/Wi-Fi SSID: ");
  Serial.println(wifi_ssid);

  // Wait up to 20 seconds for mobile hotspot DHCP handshake
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 40) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[SUCCESS] Hotspot Connected!");
    Serial.print("Station IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("Streaming real-time telemetry to Render website...");
  } else {
    Serial.println("\n[NOTICE] Hotspot did not connect within 20 seconds.");
    Serial.printf("Status Code: %d (3 = Connected, 4 = Failed, 6 = Disconnected)\n", WiFi.status());
    Serial.println("Tip: Check that phone hotspot is on 2.4 GHz band (not 5 GHz).");
    Serial.println("SoftAP fallback is running for local presentation.");
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
  server.on("/api/data", HTTP_OPTIONS, handleOptions);
  server.on("/api/thresholds", HTTP_POST, handleApiThresholds);
  server.on("/api/thresholds", HTTP_OPTIONS, handleOptions);
  server.enableCORS(true);

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
    } else {
      humidityPct = 0.0;
      temperatureC = 0.0;
    }

    // Print to Serial Monitor for debugging
    String activeIp = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "192.168.4.1 (Hotspot)";
    Serial.printf("ADC: %d | Volt: %.2fV | Temp: %.1fC | Hum: %.1f%% | Status: %s | Web IP: %s\n",
                  rawAdc, voltage, temperatureC, humidityPct, systemStatus.c_str(), activeIp.c_str());

    // Update OLED Display
    updateOLED();

    // Push to Cloud Website (if configured)
    sendTelemetryToCloud();
  }

  // 3. Update Actuators (LEDs and Buzzer)
  updateActuatorsAndStatus();
}

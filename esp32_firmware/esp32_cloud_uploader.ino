/*
 * =========================================================================================
 * Project: ESP32 Cloud Uploader - Real-Time Air Quality Telemetry to Web
 * 
 * Hardware Connections (Streamlined Breadboard Setup):
 *   - DHT11 Sensor:      GND -> GND, VCC -> 3.3V, DATA -> GPIO 12
 *   - MQ-135 Gas Sensor: GND -> GND, VCC -> 5V,   A0 -> GPIO 34 (via voltage divider)
 *   - Active Buzzer:     Negative (-) -> GND,     Positive (+) -> GPIO 21
 * 
 * Note: OLED and physical LEDs are omitted to save breadboard space.
 * All visual monitoring, gauges, and warning indicators are displayed in real-time
 * on the Web Dashboard!
 * =========================================================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

// ================= PINS CONFIGURATION =================
#define PIN_MQ135         34  // Analog Input (ADC1_CH6 via voltage divider)
#define PIN_DHT           12  // Digital pin for DHT11
#define PIN_BUZZER        21  // Active Buzzer Positive

#define DHTTYPE           DHT11

// ================= NETWORK & CLOUD CONFIGURATION =================
const char* wifi_ssid     = "YOUR_WIFI_NAME";     // <-- Enter your Wi-Fi SSID
const char* wifi_password = "YOUR_WIFI_PASSWORD"; // <-- Enter your Wi-Fi Password

// Cloud / Local Server Endpoint
// E.g.: "https://your-render-app.onrender.com/api/telemetry" or "http://192.168.1.50:3000/api/telemetry"
const char* cloudServerUrl = "https://your-render-app.onrender.com/api/telemetry";

// Thresholds
int thresholdWarning = 1300;   // Warning Level -> Web dashboard flags Yellow
int thresholdAlert   = 2100;   // Critical Level -> Buzzer sounds + Web flags Red

// Hardware Objects
DHT dht(PIN_DHT, DHTTYPE);

// Telemetry state variables
int rawAdc = 0;
float voltage = 0.0;
int estimatedPpm = 400;
float temperatureC = 0.0;
float humidityPct = 0.0;
String systemStatus = "NORMAL"; // NORMAL | WARNING | ALERT

unsigned long lastUploadTime = 0;
const unsigned long UPLOAD_INTERVAL = 2000; // 2 seconds between cloud pushes
unsigned long buzzerBlinkTime = 0;
bool buzzerToggle = false;

// Filtered ADC reading (16-sample oversampling)
int readMQ135Filtered() {
  long sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(PIN_MQ135);
    delayMicroseconds(500);
  }
  return sum / 16;
}

// Approximate PPM equation
int calculateEstimatedPpm(int adc) {
  if (adc < 600) return 400;
  float ratio = (float)adc / 600.0;
  int ppm = 400 + (int)(pow(ratio, 2.5) * 85.0);
  if (ppm > 9999) ppm = 9999;
  return ppm;
}

// Push JSON telemetry payload to Cloud Server
void sendTelemetryToCloud() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Cloud] WiFi not connected. Skipping upload.");
    return;
  }

  HTTPClient http;
  http.begin(cloudServerUrl);
  http.addHeader("Content-Type", "application/json");

  // Construct JSON payload
  String jsonPayload = "{";
  jsonPayload += "\"adc\":" + String(rawAdc) + ",";
  jsonPayload += "\"ppm\":" + String(estimatedPpm) + ",";
  jsonPayload += "\"voltage\":" + String(voltage, 2) + ",";
  jsonPayload += "\"temp\":" + String(isnan(temperatureC) ? 0.0 : temperatureC, 1) + ",";
  jsonPayload += "\"hum\":" + String(isnan(humidityPct) ? 0.0 : humidityPct, 1) + ",";
  jsonPayload += "\"status\":\"" + systemStatus + "\",";
  jsonPayload += "\"buzz\":" + String(systemStatus == "ALERT" ? 1 : 0);
  jsonPayload += "}";

  int httpCode = http.POST(jsonPayload);
  if (httpCode > 0) {
    Serial.printf("[Cloud] Upload success! Code: %d | ADC: %d | Temp: %.1fC | Hum: %.1f%% | Status: %s\n",
                  httpCode, rawAdc, temperatureC, humidityPct, systemStatus.c_str());
  } else {
    Serial.printf("[Cloud] Upload failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

// Update Buzzer and System Condition
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

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- ESP32 Air Quality Monitoring (Streamlined Setup) ---");

  // Buzzer Setup
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  // Initialize DHT11
  dht.begin();

  // Configure ADC for MQ-135 on GPIO 34
  analogReadResolution(12); // 12-bit: 0 - 4095
  analogSetAttenuation(ADC_11db); // 0 - 3.3V

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  Serial.print("Connecting to Wi-Fi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi Connected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWi-Fi Connection failed. Will retry in loop.");
  }
}

void loop() {
  unsigned long now = millis();

  // 1. Read Sensors every 2 seconds
  if (now - lastUploadTime >= UPLOAD_INTERVAL) {
    lastUploadTime = now;

    rawAdc = readMQ135Filtered();
    voltage = (rawAdc / 4095.0) * 3.3;
    estimatedPpm = calculateEstimatedPpm(rawAdc);

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) temperatureC = t;
    if (!isnan(h)) humidityPct = h;

    // Check Wi-Fi reconnection if dropped
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reconnecting Wi-Fi...");
      WiFi.reconnect();
    } else {
      sendTelemetryToCloud();
    }
  }

  // 2. Actuator logic (Buzzer alarm)
  updateBuzzerAndStatus();
}

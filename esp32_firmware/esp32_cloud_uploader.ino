/*
 * =========================================================================================
 * Project: ESP32 Cloud Uploader - Real-Time Air Quality Telemetry to Web
 * Uploads Analog Sensor Data (MQ-135, DHT11) to Cloud Server or Local Web Server
 * 
 * Hardware Connections:
 *   - DHT11 Sensor:        GND -> GND, VCC -> 3.3V, DATA (D) -> GPIO 12
 *   - MQ-135 Gas Sensor:   GND -> GND, VCC -> 5V,   A0 -> GPIO 34 (via 10k/20k voltage divider)
 *   - Active Buzzer:       Negative (-) -> GND,     Positive (P) -> GPIO 21
 *   - Green LED (Safe):    Cathode (-) -> GND,      Anode (+) -> GPIO 18 (via 220-330 ohm resistor)
 *   - Red LED (Warning):   Cathode (-) -> GND,      Anode (+) -> GPIO 19 (via 220-330 ohm resistor)
 *   - 0.96" OLED Display:  GND -> GND, VCC -> 3.3V, SDA -> GPIO 26, SCL -> GPIO 25
 * =========================================================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ================= PINS CONFIGURATION =================
#define PIN_MQ135         34  // Analog Input (ADC1_CH6 via voltage divider)
#define PIN_DHT           12  // Digital pin for DHT11
#define PIN_BUZZER        21  // Active Buzzer Positive (P)
#define PIN_LED_GREEN     18  // Normal Condition Green LED
#define PIN_LED_RED       19  // Warning / Alert Red LED
#define PIN_OLED_SDA      26  // OLED I2C SDA (GPIO 26)
#define PIN_OLED_SCL      25  // OLED I2C SCL (GPIO 25)

#define DHTTYPE           DHT11
#define SCREEN_WIDTH      128
#define SCREEN_HEIGHT     64
#define OLED_RESET        -1
#define SCREEN_I2C_ADDR   0x3C

// ================= NETWORK & CLOUD CONFIGURATION =================
const char* wifi_ssid     = "YOUR_WIFI_NAME";     // <-- Enter your Wi-Fi SSID
const char* wifi_password = "YOUR_WIFI_PASSWORD"; // <-- Enter your Wi-Fi Password

// Cloud / Local Server Endpoint (Render Cloud Server)
const char* cloudServerUrl = "https://air-quality-monitoring-75du.onrender.com/api/telemetry";

// Device Identification
const char* deviceId      = "ESP32-Govardhan"; // Change to your friend/device name!

// Thresholds (Calibrated for your room's clean air baseline: ~2400)
int thresholdWarning = 2700;   // Warning Level -> Red LED ON, Web Warning
int thresholdAlert   = 3200;   // Critical Level -> Red LED Flashing + Buzzer Siren

// Hardware Objects
DHT dht(PIN_DHT, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledAvailable = false;

// Telemetry state variables
int rawAdc = 0;
float voltage = 0.0;
int estimatedPpm = 400;
float temperatureC = 0.0;
float humidityPct = 0.0;
String systemStatus = "NORMAL";

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

  WiFiClientSecure client;
  client.setInsecure(); // Disable SSL cert check for Render HTTPS
  HTTPClient http;

  if (!http.begin(client, cloudServerUrl)) {
    Serial.println("[Cloud] Connection failed to initialize");
    return;
  }
  http.addHeader("Content-Type", "application/json");

  // Construct JSON payload
  String jsonPayload = "{";
  jsonPayload += "\"deviceId\":\"" + String(deviceId) + "\",";
  jsonPayload += "\"adc\":" + String(rawAdc) + ",";
  jsonPayload += "\"ppm\":" + String(estimatedPpm) + ",";
  jsonPayload += "\"voltage\":" + String(voltage, 2) + ",";
  jsonPayload += "\"temp\":" + String(isnan(temperatureC) ? 0.0 : temperatureC, 1) + ",";
  jsonPayload += "\"hum\":" + String(isnan(humidityPct) ? 0.0 : humidityPct, 1) + ",";
  jsonPayload += "\"status\":\"" + systemStatus + "\",";
  jsonPayload += "\"green\":" + String(systemStatus == "NORMAL" ? 1 : 0) + ",";
  jsonPayload += "\"red\":" + String(systemStatus != "NORMAL" ? 1 : 0) + ",";
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

// Update LEDs, Buzzer and System Condition
void updateActuatorsAndStatus() {
  if (rawAdc >= thresholdAlert) {
    systemStatus = "ALERT";
    digitalWrite(PIN_LED_GREEN, LOW);
    // Intermittent alarm pulsing
    if (millis() - buzzerBlinkTime > 300) {
      buzzerBlinkTime = millis();
      buzzerToggle = !buzzerToggle;
      digitalWrite(PIN_LED_RED, buzzerToggle ? HIGH : LOW);
      digitalWrite(PIN_BUZZER, buzzerToggle ? HIGH : LOW);
    }
  } else if (rawAdc >= thresholdWarning) {
    systemStatus = "WARNING";
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, HIGH);
    digitalWrite(PIN_BUZZER, LOW);
  } else {
    systemStatus = "NORMAL";
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_RED, LOW);
    digitalWrite(PIN_BUZZER, LOW);
  }
}

// Auto-detect and initialize SSD1306 OLED at 0x3C or 0x3D
bool initOLEDDisplay() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(100000); // 100 kHz standard mode
  delay(50);

  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[OLED SUCCESS] SSD1306 OLED detected at address 0x3C!");
    return true;
  }
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
    Serial.println("[OLED SUCCESS] SSD1306 OLED detected at address 0x3D!");
    return true;
  }
  Serial.println("[OLED Notice] OLED display not detected on SDA=GPIO 26, SCL=GPIO 25.");
  return false;
}

// Update OLED Display (If attached)
bool oledHeartbeat = false;
void updateOLED() {
  // Auto-retry connecting if not detected at boot
  if (!oledAvailable) {
    static unsigned long lastOledRetry = 0;
    if (millis() - lastOledRetry > 3000) {
      lastOledRetry = millis();
      oledAvailable = initOLEDDisplay();
    }
    if (!oledAvailable) return;
  }

  oledHeartbeat = !oledHeartbeat;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("CLOUD IOT [");
  display.print(systemStatus);
  display.print("]");
  if (oledHeartbeat) {
    display.fillCircle(123, 4, 2, SSD1306_WHITE);
  }
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(0, 15);
  display.print("Gas ADC: ");
  display.print(rawAdc);
  display.print(" (");
  display.print(estimatedPpm);
  display.print("ppm)");

  display.setCursor(0, 28);
  display.print("Temp: ");
  display.print(temperatureC, 1);
  display.print(" C");

  display.setCursor(0, 41);
  display.print("Hum:  ");
  display.print(humidityPct, 1);
  display.print(" %");

  display.drawLine(0, 53, 127, 53, SSD1306_WHITE);
  display.setCursor(0, 56);
  display.print(WiFi.status() == WL_CONNECTED ? "WiFi: Connected" : "WiFi: Reconnecting");

  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- ESP32 Cloud Air Quality Monitor Starting ---");

  // Actuator Pins Setup
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  digitalWrite(PIN_LED_GREEN, HIGH);
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  // Initialize Custom I2C (SDA = GPIO 26, SCL = GPIO 25)
  oledAvailable = initOLEDDisplay();
  if (oledAvailable) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(10, 25);
    display.println("Connecting Cloud...");
    display.display();
  }

  // Initialize DHT11
  dht.begin();

  // Configure ADC for MQ-135 on GPIO 34
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

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

  // 1. Read Sensors every 2 seconds & push to cloud
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

    updateOLED();
  }

  // 2. Actuator logic (LEDs and Buzzer)
  updateActuatorsAndStatus();
}

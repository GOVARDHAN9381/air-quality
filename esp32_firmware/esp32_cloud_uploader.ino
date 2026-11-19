/*
 * =========================================================================================
 * Project: ESP32 Cloud Uploader - Real-Time Air Quality Telemetry to Web
 * Uploads Analog Sensor Data (MQ-135, DHT11) to Cloud Server or Local Web Server
 * =========================================================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ================= PINS CONFIGURATION =================
#define PIN_MQ135         34  // Analog Input (via voltage divider)
#define PIN_DHT           12  // Digital pin for DHT11
#define PIN_BUZZER        21  // Active Buzzer Positive
#define PIN_LED_GREEN     18  // Normal Indicator
#define PIN_LED_RED       19  // Warning / Alert Indicator
#define PIN_OLED_SDA      22  // I2C SDA
#define PIN_OLED_SCL      23  // I2C SCL

#define DHTTYPE           DHT11
#define SCREEN_WIDTH      128
#define SCREEN_HEIGHT     64
#define OLED_RESET        -1
#define SCREEN_I2C_ADDR   0x3C

// ================= NETWORK & CLOUD CONFIGURATION =================
const char* wifi_ssid     = "YOUR_WIFI_NAME";
const char* wifi_password = "YOUR_WIFI_PASSWORD";

// Target Cloud / Local Server Endpoint
// E.g.: "http://192.168.1.50:3000/api/telemetry" (Local PC)
// Or:   "https://airsense-esp32.onrender.com/api/telemetry" (Render Cloud)
const char* cloudServerUrl = "http://192.168.1.50:3000/api/telemetry";

// Thresholds
int thresholdWarning = 1300;
int thresholdAlert   = 2100;

// Hardware Objects
DHT dht(PIN_DHT, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// State variables
int rawAdc = 0;
float temperatureC = 0.0;
float humidityPct = 0.0;
String systemStatus = "NORMAL";

unsigned long lastUploadTime = 0;
const unsigned long UPLOAD_INTERVAL = 2000; // 2 seconds between cloud pushes

// Filtered ADC reading
int readMQ135Filtered() {
  long sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(PIN_MQ135);
    delayMicroseconds(500);
  }
  return sum / 16;
}

// Push JSON telemetry payload to Cloud Server
void sendTelemetryToCloud() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Skipping upload.");
    return;
  }

  HTTPClient http;
  http.begin(cloudServerUrl);
  http.addHeader("Content-Type", "application/json");

  // Construct JSON payload
  String jsonPayload = "{";
  jsonPayload += "\"adc\":" + String(rawAdc) + ",";
  jsonPayload += "\"temp\":" + String(temperatureC, 1) + ",";
  jsonPayload += "\"hum\":" + String(humidityPct, 1) + ",";
  jsonPayload += "\"green\":" + String(systemStatus == "NORMAL" ? 1 : 0) + ",";
  jsonPayload += "\"red\":" + String(systemStatus != "NORMAL" ? 1 : 0) + ",";
  jsonPayload += "\"buzz\":" + String(systemStatus == "ALERT" ? 1 : 0);
  jsonPayload += "}";

  int httpCode = http.POST(jsonPayload);
  if (httpCode > 0) {
    Serial.printf("[Cloud] Upload success! HTTP Code: %d\n", httpCode);
  } else {
    Serial.printf("[Cloud] Upload failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

void updateActuators() {
  if (rawAdc >= thresholdAlert) {
    systemStatus = "ALERT";
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, HIGH);
    digitalWrite(PIN_BUZZER, HIGH);
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

void updateOLED() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("CLOUD IOT [");
  display.print(systemStatus);
  display.print("]");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(0, 15);
  display.print("Gas ADC: ");
  display.print(rawAdc);

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
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  digitalWrite(PIN_LED_GREEN, HIGH);
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_I2C_ADDR);
  dht.begin();

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  WiFi.begin(wifi_ssid, wifi_password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
}

void loop() {
  unsigned long now = millis();
  if (now - lastUploadTime >= UPLOAD_INTERVAL) {
    lastUploadTime = now;

    rawAdc = readMQ135Filtered();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) temperatureC = t;
    if (!isnan(h)) humidityPct = h;

    updateActuators();
    updateOLED();
    sendTelemetryToCloud();
  }
}

# Complete Deployment Guide: Sharing ESP32 Analog Values to the Web

This guide provides 3 proven deployment methods to share real-time analog air quality values, temperature, humidity, and warning statuses from your ESP32 to a web browser.

---

## Architecture Overview

```
 [MQ-135 Gas Sensor] (Analog 0-5V) 
          | 
   [Voltage Divider] (Drops to 0-3.3V)
          |
     (GPIO 34)
          v
  +-------------------------------------------------------+
  |                   ESP32 DevKit V1                     |
  |  - ADC1_CH6 Oversampling (12-bit, 0-4095)             |
  |  - DHT11 Read (GPIO 12)                               |
  |  - Threshold Decision Engine (Normal / Warn / Alert)  |
  |  - OLED Display 128x64 (GPIO 22/23)                   |
  |  - Actuators: Green LED (18), Red LED (19), Buzzer (21)|
  +-------------------------------------------------------+
          |
          +-----------------------+-----------------------+
          | (Method 1: Direct)    | (Method 2: Cloud API) | (Method 3: IoT Cloud)
          v                       v                       v
   [Local Web Server]     [Cloud Server (Render)]   [ThingSpeak / Adafruit]
   http://192.168.4.1     https://airsense.app      api.thingspeak.com
          |                       |                       |
          +-----------------------+-----------------------+
                                  |
                                  v
                    [Interactive Web Dashboard]
                     - Real-time Gauges (PPM, ADC, Voltage)
                     - Environmental KPIs (Temp, Humidity, Heat Index)
                     - Dynamic Chart.js Live Graphs
                     - Visual & Audio Warning Sirens
                     - Telemetry Log & CSV Export
```

---

## Method 1: Local ESP32 Web Server (Recommended for Class Viva & Demos)

**Why choose this method?**
- Zero external servers, zero cloud bills, zero internet required.
- The ESP32 itself hosts the web server and serves the dashboard.
- If your college lab does not have Wi-Fi or blocks peer connections, the ESP32 creates its own Wi-Fi Hotspot!

### Step 1: Flash the Code
1. Open Arduino IDE.
2. Open [`esp32_firmware/esp32_air_quality.ino`](file:///c:/airquality/esp32_firmware/esp32_air_quality.ino).
3. If connecting to your home router or mobile hotspot, enter your credentials:
   ```cpp
   const char* wifi_ssid     = "YOUR_WIFI_NAME";
   const char* wifi_password = "YOUR_WIFI_PASSWORD";
   ```
4. Click **Upload**.

### Step 2: Accessing the Web Dashboard
- **Case A (Connected to Wi-Fi Router):**
  1. Open Serial Monitor (115200 baud). The ESP32 prints its local IP, e.g.:
     `Station IP Address: 192.168.1.45`
  2. Notice that this exact IP is also displayed directly on line 4 of the 0.96" OLED screen!
  3. Open any browser on your laptop or phone (connected to the same Wi-Fi) and navigate to:
     `http://192.168.1.45`
  4. You will see the live embedded dashboard updating every 1.5 seconds!

- **Case B (Standalone College Lab Hotspot):**
  1. If no Wi-Fi is available, the ESP32 broadcasts its own Wi-Fi network:
     - **SSID:** `AirSense-ESP32-Hotspot`
     - **Password:** `12345678`
  2. Connect your laptop or phone to `AirSense-ESP32-Hotspot`.
  3. Open your browser and go to:
     `http://192.168.4.1`

---

## Method 2: Global Cloud Deployment (Access from Anywhere in the World)

**Why choose this method?**
- Anyone in the world can open `https://your-app.onrender.com` on their smartphone or PC and monitor your ESP32 in real time.
- The ESP32 sends HTTP POST requests whenever connected to any Wi-Fi with internet access.

### Step 1: Deploy the Backend & Dashboard to Render.com (100% Free)
1. Push this folder to a GitHub repository (e.g. `github.com/your-username/esp32-air-quality`).
2. Log in to [Render.com](https://render.com) (free account).
3. Click **New +** &rarr; **Web Service**.
4. Connect your GitHub repository.
5. Fill in the settings:
   - **Root Directory:** `cloud_server`
   - **Environment:** `Node`
   - **Build Command:** `npm install`
   - **Start Command:** `node server.js`
   - **Instance Type:** `Free`
6. Click **Deploy Web Service**.
7. In ~2 minutes, Render gives you a live public HTTPS URL:
   `https://airsense-iot-demo.onrender.com`

### Step 2: Configure ESP32 Cloud Firmware
1. Open [`esp32_firmware/esp32_cloud_uploader.ino`](file:///c:/airquality/esp32_firmware/esp32_cloud_uploader.ino).
2. Set your Wi-Fi credentials and your new cloud URL:
   ```cpp
   const char* wifi_ssid      = "YOUR_WIFI_NAME";
   const char* wifi_password  = "YOUR_WIFI_PASSWORD";
   const char* cloudServerUrl = "https://airsense-iot-demo.onrender.com/api/telemetry";
   ```
3. Upload to your ESP32.
4. Your ESP32 will now push analog telemetry every 2 seconds to the cloud server, and your Render dashboard will broadcast it to all connected viewers in real time via WebSockets!

---

## Method 3: Running Locally on Your PC (During Development & Testing)

If you are testing at your desk and want to run the server on your Windows PC:

### Option A: Using Vanilla Node.js (Zero npm install needed)
Open PowerShell and run:
```powershell
cd c:\airquality\cloud_server
node server_builtin.js
```
Open `http://localhost:3000` in your web browser.

### Option B: Using Python 3 (Zero pip install needed)
Open PowerShell and run:
```powershell
cd c:\airquality\cloud_server
python server.py
```
Open `http://localhost:3000` in your web browser.

### Option C: Direct Browser Opening (No server needed!)
1. Open Windows File Explorer.
2. Navigate to `c:\airquality\web_dashboard`.
3. Double-click `index.html`.
4. It opens in Chrome/Edge immediately.
5. You can use **Demo Mode** to simulate gas spikes, test the buzzer alarm, or enter your ESP32's local IP in the top bar to connect directly!

---

## Method 4: Free IoT Platform (ThingSpeak)

If your teacher prefers standard academic IoT platforms like ThingSpeak:

1. Create a free account at [ThingSpeak.com](https://thingspeak.com).
2. Create a **New Channel**:
   - Field 1: `MQ135_ADC`
   - Field 2: `Temperature`
   - Field 3: `Humidity`
   - Field 4: `PPM`
3. Copy your **Write API Key**.
4. In your ESP32 sketch, add:
   ```cpp
   #include <HTTPClient.h>
   
   void sendToThingSpeak(int adc, float temp, float hum) {
     if (WiFi.status() == WL_CONNECTED) {
       HTTPClient http;
       String url = "http://api.thingspeak.com/update?api_key=YOUR_WRITE_API_KEY"
                  + "&field1=" + String(adc)
                  + "&field2=" + String(temp)
                  + "&field3=" + String(hum);
       http.begin(url);
       int code = http.GET();
       http.end();
     }
   }
   ```
5. Note: ThingSpeak free tier has a 15-second rate limit between uploads.

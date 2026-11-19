# ESP32 Indoor Air Quality & Environmental Sensing Hub
### Course Project: Microprocessors & Microcontrollers (MPMC T-3)

An end-to-end IoT embedded solution for real-time indoor air quality monitoring, environmental sensing (temperature & humidity), intelligent multi-stage hazard alerting, and live web visualization.

---

## 📁 Repository & Workspace Directory Structure

```
c:\airquality\
├── esp32_firmware/
│   ├── esp32_air_quality.ino        # Full Standalone Firmware (OLED, Sensors, LEDs, Buzzer + On-board Web Server)
│   └── esp32_cloud_uploader.ino     # Cloud Uploader Firmware (Pushes JSON telemetry to Web/Cloud)
│
├── web_dashboard/                   # Responsive Real-Time Web Application
│   ├── index.html                   # Dark-themed glassmorphism UI with live gauges & warning banners
│   ├── style.css                    # Modern CSS3 styling (Mobile, Tablet & Projector optimized)
│   └── app.js                       # Real-time Chart.js graphs, Web Audio alarm, Demo simulator, CSV export
│
├── cloud_server/                    # Turn-key Telemetry Hub & API Server
│   ├── server_builtin.js            # Zero-dependency Node.js server (Runs with `node server_builtin.js`)
│   ├── server.py                    # Zero-dependency Python server (Runs with `python server.py`)
│   ├── server.js                    # Full Socket.io Express server (for Render.com / Railway)
│   └── package.json                 # Cloud configuration
│
└── docs/                            # Comprehensive Documentation & Exam Prep
    ├── CIRCUIT_DIAGRAM_AND_PINOUT.md # Exact pin matrix, 5V-to-3.3V divider math, GPIO 12 strapping guide
    ├── DEPLOYMENT_GUIDE.md          # 3 Deployment methods (Local AP, Global Cloud, ThingSpeak)
    ├── PROJECT_EXPLANATION.md       # Complete 6-page MPMC presentation transcript and slide walkthrough
    └── VIVA_QUESTIONS_AND_ANSWERS.md # 25 Technical viva questions with high-scoring model answers
```

---

## ⚡ Quick Start: 3 Ways to Run & Demonstrate

### 1. Instant Demo Mode (No Hardware Required - Perfect for Class Presentation)
1. Open [`c:\airquality\web_dashboard\index.html`](file:///c:/airquality/web_dashboard/index.html) in Google Chrome or Microsoft Edge.
2. The dashboard immediately boots in **Demo Simulation Mode** with realistic gas fluctuations, live Chart.js graphs, MQ-135 warmup timer, and simulated warning/critical alarm events!
3. Click **"Test Alarm Sequence"** to demonstrate how the warning banner turns red, the buzzer triggers, and the web audio siren sounds!

### 2. Standalone ESP32 On-Board Web Server (During Viva in College)
1. Flash [`esp32_firmware/esp32_air_quality.ino`](file:///c:/airquality/esp32_firmware/esp32_air_quality.ino) to your ESP32.
2. The ESP32 hosts its own Wi-Fi Hotspot:
   - **SSID:** `AirSense-ESP32-Hotspot`
   - **Password:** `12345678`
3. Connect your laptop or phone to this hotspot and open `http://192.168.4.1` in your browser.
4. The dashboard streams real-time analog readings directly from the ESP32!

### 3. Global Cloud Deployment (Access from Anywhere via Internet)
1. Deploy the `cloud_server` folder to [Render.com](https://render.com) for free.
2. Flash [`esp32_firmware/esp32_cloud_uploader.ino`](file:///c:/airquality/esp32_firmware/esp32_cloud_uploader.ino) with your Render URL.
3. Anyone in the world can open your public website URL and view your live air quality readings!

---

## 🔌 Hardware Pinout Summary

| Component | Pin | ESP32 Pin | Purpose |
| :--- | :--- | :--- | :--- |
| **MQ-135** | A0 | **GPIO 34** | Analog gas input (via 10kΩ/20kΩ voltage divider to stay &le; 3.3V) |
| **MQ-135** | VCC & GND | **5V & GND** | Heater coil requires 5V to maintain 200°C operational temp |
| **DHT11** | Data (D) | **GPIO 12** | Single-bus digital temperature and humidity reading |
| **DHT11** | VCC & GND | **3.3V & GND** | Standard 3.3V power |
| **OLED (SSD1306)** | SDA | **GPIO 22** | Custom I2C Data wire |
| **OLED (SSD1306)** | SCL | **GPIO 23** | Custom I2C Clock wire |
| **Green LED** | Anode (+) | **GPIO 18** | Normal Safe Status indicator (via 220Ω resistor) |
| **Red LED** | Anode (+) | **GPIO 19** | Warning / Alert Status indicator (via 220Ω resistor) |
| **Active Buzzer** | Positive (+) | **GPIO 21** | Audible siren during critical gas concentration |

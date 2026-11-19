# Complete Project Explanation: MPMC T-3 Presentation (6 Pages)

**Project Title:** ESP32-Based Real-Time Indoor Air Quality Monitoring, Environmental Sensing and Intelligent Warning System  
**Presentation Track:** Microprocessors and Microcontrollers (MPMC T-3)  
**Hardware Configuration:** Streamlined Setup (DHT11 + MQ-135 + Buzzer + IoT Web Dashboard)  

---

## Slide 1: Project Title & Introduction

### Speech Script / Viva Explanation:
> *"Respected teachers and evaluators, today we present our project: **ESP32-Based Real-Time Indoor Air Quality Monitoring, Environmental Sensing and Intelligent Warning System**.*
>
> *Indoor air quality directly impacts human health, concentration, and everyday safety. People spend more than 85% to 90% of their daily time inside enclosed spaces—such as classrooms, homes, offices, and laboratories.*
>
> *Often, indoor air becomes polluted due to the buildup of harmful gases like carbon dioxide, carbon monoxide, ammonia, volatile organic compounds, and smoke, along with changes in temperature and relative humidity.*
>
> *Because human senses cannot reliably detect gradual toxic gas accumulation, we engineered an automated edge IoT monitoring system using the ESP32 microcontroller. The system samples environmental air quality in real time, triggers an on-board acoustic alarm via an active buzzer during hazardous spikes, and streams live telemetry to a rich interactive web dashboard accessible on any smartphone or laptop."*

### Key Slide Bullets:
- Continuous real-time sensing of harmful airborne gases, temperature, and humidity.
- Physical acoustic warning via active buzzer + real-time cloud/web dashboard.
- Streamlined hardware implementation eliminating breadboard congestion.

---

## Slide 2: Objectives & Problem Statement

### The Problem:
1. **Silent Accumulation:** Toxic gases like $\text{CO}_2$ or chemical fumes rise silently, causing fatigue, headaches, and respiratory issues without immediate detection.
2. **Poor Indoor Ventilation:** In closed air-conditioned classrooms and offices, fresh oxygen decreases rapidly.
3. **Ineffective Manual Testing:** Manual chemical tests are periodic and cannot provide immediate alerts during sudden leaks or fire hazards.

### Objectives:
- Real-time acquisition of harmful gas levels using the **MQ-135 sensor (GPIO 34)**.
- Continuous sensing of ambient temperature and relative humidity using **DHT11 (GPIO 12)**.
- Edge data processing and moving-average filtering on the **ESP32**.
- Instant audible warnings using an **Active Buzzer (GPIO 21)**.
- Remote telemetry streaming to an **Interactive Web Dashboard** over local Wi-Fi or Cloud (Render).

---

## Slide 3: Hardware Components & Architecture

| Component | Role in Project | Technical Specification & Connection |
| :--- | :--- | :--- |
| **ESP32 DevKit V1** | Master Edge Controller | Dual-core 32-bit Xtensa @ 240 MHz, 520 KB SRAM, 12-bit ADC, Integrated 2.4 GHz Wi-Fi & Bluetooth |
| **MQ-135 Gas Sensor** | Air Quality & Harmful Gas Sensing | Sensitive to $\text{NH}_3$, $\text{NO}_x$, alcohol, benzene, smoke, $\text{CO}_2$. **Connections: A0 &rarr; GPIO 34 (via divider), V &rarr; 5V, G &rarr; GND** |
| **DHT11 Sensor** | Temperature & Humidity Sensing | Measures 0°C–50°C and 20%–90% RH. **Connections: D &rarr; GPIO 12, V &rarr; 3.3V, G &rarr; GND** |
| **Active Buzzer** | Acoustic Warning Siren | Generates pulsed audio siren during critical alerts. **Connections: P &rarr; GPIO 21, Negative &rarr; GND** |
| **IoT Web Dashboard** | Visual Display & Trend Logging | Replaces physical OLED/LEDs to save breadboard space; displays live gauges, graphs, and warnings |

---

## Slide 4: 5-Step Working Principle & The 3 Main Modules

```
[Module 1: Sensing]          [Module 2: Acquisition & Edge Processing]       [Module 3: Intelligent Alert & Web]
+----------------------+     +-----------------------------------------+     +--------------------------------+
| MQ-135 (GPIO 34 ADC) | --> | Step 1: 16-sample Oversampling & Filter | --> | Active Buzzer (GPIO 21 - Alert)|
| DHT11 (GPIO 12 1-Wire)|     | Step 2: Threshold Comparison Engine     |     | Web Dashboard (HTML5 / REST)   |
+----------------------+     | Step 3: Condition Classification        |     | Real-time Chart.js Trends      |
                             +-----------------------------------------+     +--------------------------------+
```

### Module 1: Indoor Air Quality & Environmental Sensing
- **Step 1 (Sensor Data Collection):** MQ-135 reacts electrochemically to airborne pollutants. DHT11 samples humidity and temperature every 2 seconds.

### Module 2: Real-Time Data Acquisition & Processing
- **Step 2 (Data Acquisition):** ESP32 digitizes the analog voltage on GPIO 34 using its internal 12-bit ADC ($0 - 4095$) and reads the digital packet from DHT11 on GPIO 12.
- **Step 3 (Edge Processing & State Classification):** The raw reading is compared against calibrated decision boundaries:
  - $\text{ADC} < 1300$: **Normal State** (Web shows Green Safe banner, Buzzer silent)
  - $1300 \le \text{ADC} < 2100$: **Warning State** (Web shows Yellow Warning banner, Buzzer silent)
  - $\text{ADC} \ge 2100$: **Critical Alert State** (Web flashes Red Alert banner, Buzzer sounds pulsed alarm siren)

### Module 3: Intelligent Air Quality Alert & Visualization
- **Step 4 (Acoustic Annunciation):** Active buzzer on GPIO 21 sounds if air quality exceeds the critical alert threshold.
- **Step 5 (Web & IoT Visualization):** The ESP32 serves/streams real-time telemetry to the web dashboard, plotting live curves with Chart.js and logging data.

---

## Slide 5: Actual Hardware Interfacing & Electrical Wiring

```
                       ESP32 DEVKIT V1
                      +----------------+
                      |                |
(5V Heated Output)    |                |
MQ-135 [A0] --[10k]---+---> GPIO 34    | (ADC1 Input, 0-3.3V safe)
               |      |                |
             [20k]    |                |
               |      |                |
              GND     |                |
MQ-135 [VCC] ---------+---> 5V (VIN)   | (5V required for heater coil)
MQ-135 [GND] ---------+---> GND        |
                      |                |
DHT11 [Data] ---------+---> GPIO 12    | (Single-bus Digital Input)
DHT11 [VCC] ----------+---> 3.3V       |
DHT11 [GND] ----------+---> GND        |
                      |                |
Buzzer [Positive] ----+---> GPIO 21    | (Active Siren Digital Out)
Buzzer [Negative] ----+---> GND        |
                      +----------------+
```

### Engineering Solutions:
1. **Voltage Protection for GPIO 34:** A 10kΩ / 20kΩ resistor divider attenuates MQ-135's 5V output down to safe 3.33V maximum to protect the ESP32 ADC.
2. **Boot Pin Isolation on GPIO 12:** DHT11 connects to GPIO 12 without interfering with the MTDI strapping boot sequence.
3. **Space Optimization:** Physical OLED and LEDs are replaced by the Web Visualization Dashboard, keeping the breadboard clean, compact, and reliable.

---

## Slide 6: Results, Advantages, Applications & Future Scope

### Key Advantages:
- **Clean Breadboard Layout:** Eliminates messy wiring by consolidating visual display into the Web Dashboard.
- **Autonomous & Real-Time:** 2-second continuous refresh rate eliminates manual inspection.
- **Dual Visual & Audible Feedback:** Clear web alerts + physical acoustic buzzer alarm.
- **Fail-Safe Wi-Fi Architecture:** Operates on local Wi-Fi router, cloud (Render), or standalone Hotspot (`192.168.4.1`).

### Practical Applications:
- Smart homes and apartment living rooms.
- School classrooms, university lecture halls, and laboratories.
- Corporate offices, conference rooms, and server rooms.

### Future Scope:
- Integration of 5V relay module for automatic exhaust fan activation.
- Optical NDIR carbon-dioxide sensors for laboratory-grade precision.
- Push notification service integration (Telegram / WhatsApp bot alerts).

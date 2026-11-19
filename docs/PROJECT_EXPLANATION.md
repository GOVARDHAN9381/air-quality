# Complete Project Explanation: MPMC T-3 Presentation (6 Pages)

**Project Title:** ESP32-Based Real-Time Indoor Air Quality Monitoring, Environmental Sensing and Intelligent Warning System  
**Presentation Track:** Microprocessors and Microcontrollers (MPMC T-3)  
**Target Platform:** ESP32 Microcontroller + MQ-135 + DHT11 + SSD1306 OLED + IoT Web Dashboard  

---

## Slide 1: Project Title & Introduction

### Speech Script / Viva Explanation:
> *"Respected teachers and evaluators, today we present our project: **ESP32-Based Real-Time Indoor Air Quality Monitoring, Environmental Sensing and Intelligent Warning System**.*
>
> *Indoor air quality is one of the most critical factors affecting human health, cognitive productivity, and everyday safety. People spend over 85% to 90% of their daily lives inside enclosed environments—such as classrooms, homes, offices, and laboratories.*
>
> *Often, indoor air becomes toxic due to the accumulation of harmful gases like carbon dioxide, carbon monoxide, volatile organic compounds, ammonia, and smoke, as well as fluctuating temperature and relative humidity.*
>
> *Because human senses cannot reliably detect gradual changes in toxic gas concentrations, we developed this automated embedded system powered by the ESP32 microcontroller. The system monitors air quality and ambient conditions in real time, displays readings locally on an OLED screen, triggers instant visual and audible alarms using LEDs and a buzzer, and streams telemetry directly to an interactive real-time web dashboard."*

### Key Slide Bullets:
- Continuous real-time monitoring of indoor pollutants and ambient climate.
- Dual-layer notification: On-board physical alerts (OLED, LEDs, Buzzer) + Cloud/Local Web Visualization.
- Fully autonomous decision-making using edge processing on the ESP32.

---

## Slide 2: Objectives & Problem Statement

### The Problem:
1. **Silent & Odorless Threats:** Hazardous gases such as elevated $\text{CO}_2$ or invisible chemical vapours rise imperceptibly, causing headaches, drowsiness, respiratory distress, and long-term illness.
2. **Poor Ventilation:** In closed air-conditioned classrooms and offices, fresh oxygen depletion occurs rapidly without people noticing.
3. **Ineffectiveness of Manual Inspection:** Manual chemical testing is periodic, expensive, and fails to provide instantaneous warnings when a gas leak or fire occurs.

### Objectives:
- Real-time acquisition of gas concentration using the MQ-135 chemical sensor.
- Continuous sensing of ambient temperature and relative humidity using DHT11.
- Edge data processing and moving-average noise filtering on the ESP32.
- Local display on an I2C OLED screen and remote telemetry streaming over Wi-Fi.
- Intelligent multi-stage threshold alarms: Normal (Green LED), Warning (Red LED), and Critical (Red Flashing LED + Buzzer Siren).

---

## Slide 3: Hardware Components & Architecture

| Component | Role in Project | Technical Specification |
| :--- | :--- | :--- |
| **ESP32 DevKit V1** | Master Edge Controller | Dual-core 32-bit Xtensa @ 240 MHz, 520 KB SRAM, 12-bit ADC, Integrated 2.4 GHz Wi-Fi & Bluetooth |
| **MQ-135 Gas Sensor** | Air Quality & Harmful Gas Sensing | Metal-oxide ($\text{SnO}_2$) semiconductor; sensitive to $\text{NH}_3$, $\text{NO}_x$, alcohol, benzene, smoke, $\text{CO}_2$ |
| **DHT11 Sensor** | Temperature & Humidity Sensing | Single-wire digital protocol; measures 0°C–50°C (±2°C) and 20%–90% RH (±5% RH) |
| **0.96" OLED Display** | Local Real-Time Display | 128x64 pixels monochrome graphic display, SSD1306 driver, Fast I2C bus |
| **Bicolor / Dual LEDs** | Visual Status Indication | Green LED for Normal safe baseline; Red LED for Warning and Hazard levels |
| **Active Buzzer** | Acoustic Warning Alarm | Piezoelectric 5V/3.3V active transducer generating distinct audio siren during critical alerts |

---

## Slide 4: 5-Step Working Principle & The 3 Main Modules

The system operates continuously through five distinct operational phases organized into three functional modules:

```
[Module 1: Sensing]          [Module 2: Acquisition & Edge Processing]       [Module 3: Intelligent Alert & Web]
+----------------------+     +-----------------------------------------+     +--------------------------------+
| MQ-135 (GPIO 34 ADC) | --> | Step 1: 16-sample Oversampling & Filter | --> | OLED Display (GPIO 22/23)      |
| DHT11 (GPIO 12 1-Wire)|     | Step 2: Threshold Comparison Engine     |     | Green LED (GPIO 18 - Safe)     |
+----------------------+     | Step 3: Condition Classification        |     | Red LED (GPIO 19 - Warning)    |
                             +-----------------------------------------+     | Active Buzzer (GPIO 21 - Alert)|
                                                                             | Web Dashboard (HTML5 / REST)   |
                                                                             +--------------------------------+
```

### Module 1: Indoor Air Quality & Environmental Sensing
- **Step 1 (Sensor Data Collection):** MQ-135 reacts electrochemically to airborne pollutants. DHT11 samples humidity and temperature every 2 seconds.

### Module 2: Real-Time Data Acquisition & Processing
- **Step 2 (Data Acquisition):** ESP32 digitizes the analog voltage on GPIO 34 using its internal 12-bit ADC ($0 - 4095$) and reads the 40-bit serial packet from DHT11 on GPIO 12.
- **Step 3 (Edge Processing & State Classification):** The raw reading is compared against calibrated decision boundaries:
  - $\text{ADC} < 1300$: **Normal State**
  - $1300 \le \text{ADC} < 2100$: **Warning State**
  - $\text{ADC} \ge 2100$: **Critical Alert State**

### Module 3: Intelligent Air Quality Alert & Visualization
- **Step 4 (Local Annunciation):** ESP32 updates the OLED screen and drives the LEDs and buzzer accordingly.
- **Step 5 (Web & IoT Visualization):** The ESP32 serves an interactive web dashboard over Wi-Fi, plotting live trend graphs with Chart.js and sounding web alarms.

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
                      |                |
DHT11 [Data] ---------+---> GPIO 12    | (Single-bus Digital Input)
                      |                |
Buzzer [+] -----------+---> GPIO 21    | (Active Siren Digital Out)
                      |                |
Green LED [Anode] ----+---> GPIO 18    | (via 220Ω Resistor)
Red LED [Anode] ------+---> GPIO 19    | (via 220Ω Resistor)
                      |                |
OLED [SDA] -----------+---> GPIO 22    | (Custom Wire I2C Data)
OLED [SCL] -----------+---> GPIO 23    | (Custom Wire I2C Clock)
                      +----------------+
```

### Engineering Solutions to Presentation Challenges:
1. **Voltage Protection for GPIO 34:** A 10kΩ / 20kΩ resistor divider attenuates MQ-135's 5V output down to safe 3.33V maximum to prevent ESP32 ADC silicon damage.
2. **Boot Pin Isolation on GPIO 12:** Care is taken so GPIO 12 is not forced HIGH during reset, guaranteeing that the ESP32 boots properly into 3.3V flash memory mode.
3. **GPIO Remapping for OLED:** Default I2C pin 21 is repurposed for the active buzzer, so I2C is cleanly redirected to GPIO 22 and 23 via `Wire.begin(22, 23)`.

---

## Slide 6: Results, Advantages, Applications & Future Enhancements

### Key Advantages:
- **Autonomous & Real-Time:** 2-second continuous refresh rate eliminates manual inspection.
- **Dual Visual & Audible Feedback:** Clear multi-tier warnings ensure room occupants cannot miss hazardous situations.
- **Fail-Safe Wi-Fi Architecture:** Operates both on local Wi-Fi routers and as an autonomous standalone Hotspot (`192.168.4.1`) for offline examination halls.
- **Cost-Effective:** Complete system cost is a fraction of commercial industrial gas detectors.

### Practical Applications:
- Smart homes and apartment living rooms.
- School classrooms, university lecture halls, and laboratories.
- Corporate offices, conference rooms, and server server rooms.
- Hospital waiting rooms and clean rooms.

### Future Scope:
- Integration of 5V relay modules for automatic exhaust fan activation.
- Optical NDIR carbon-dioxide sensors for laboratory-grade precision.
- Push notification service integration (Telegram / WhatsApp bot alerts).

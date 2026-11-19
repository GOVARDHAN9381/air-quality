# Hardware Circuit Connections, Pinout & Electrical Design
**Project: ESP32-Based Real-Time Indoor Air Quality Monitoring System**
**Configuration: Streamlined Setup (Optimized for Breadboard Space)**

---

## 1. Updated Hardware Pinout Matrix

| Component | Component Pin | ESP32 DevKit Pin | Signal Type | Electrical Function & Precautions |
| :--- | :--- | :--- | :--- | :--- |
| **DHT11 Sensor** | **G (Ground)** | **GND** | Ground | Common system ground. |
| **DHT11 Sensor** | **V (Power)** | **3.3V** | Power (3.3V DC) | Connected to ESP32 3.3V regulated power rail. |
| **DHT11 Sensor** | **D (Data)** | **GPIO 12** | Digital 1-Wire | Single-wire bidirectional serial bus. *(Bootstrap pin MTDI; do not hold HIGH at boot).* |
| **MQ-135 Gas Sensor** | **A0 (Analog Out)** | **GPIO 34** | Analog Input (ADC1_CH6) | **Input-Only Pin**. Connected through **10kΩ/20kΩ voltage divider** to step 5V down to &le; 3.3V. |
| **MQ-135 Gas Sensor** | **G (Ground)** | **GND** | Ground | Common system ground. |
| **MQ-135 Gas Sensor** | **V (Power)** | **5V (VIN)** | Power (5V DC) | Internal $\text{SnO}_2$ heating coil requires 5V to maintain ~200°C operational temperature. |
| **Active Buzzer** | **P (Positive +)** | **GPIO 21** | Digital Output | Active siren alarm driven HIGH by ESP32 during critical air quality alerts. |
| **Active Buzzer** | **Negative (-)** | **GND** | Ground | Common system ground. |
| **Visual Dashboard** | **IoT Web App** | **Web / Wi-Fi** | Real-Time Telemetry | **Replaces physical OLED & LEDs** to prevent breadboard space congestion. Displays live gauges, graphs, and warnings. |

---

## 2. Why No Physical OLED and LEDs? (Viva Talking Point)

1. **Breadboard Space Constraints:** Standard breadboards have limited pin columns. The ESP32 DevKit board itself covers most breadboard pins, leaving insufficient room for a 4-pin OLED module and discrete LEDs with current-limiting resistors.
2. **Superior IoT Web Dashboard:** Instead of relying on a tiny 0.96" display, all telemetry (PPM, voltage, temperature, humidity, and warning status) is visualized on a rich, responsive **HTML5/Chart.js Web Dashboard** accessible on smartphones and laptops.
3. **Dedicated Physical Acoustic Alarm:** The **Active Buzzer on GPIO 21** is retained as the critical physical acoustic alarm to immediately alert anyone in the room without requiring a screen.

---

## 3. Critical Engineering Safeguards

### A. The MQ-135 Voltage Divider on GPIO 34
- **The Problem:** The MQ-135 sensor requires 5V for its heating element. Under high gas or smoke concentrations, its analog output can swing up to $4.5\text{V} - 5.0\text{V}$.
- **The Danger:** ESP32 ADC inputs (like GPIO 34) tolerate a maximum input of **3.3V**. Applying 5V directly will destroy the ADC silicon!
- **The Voltage Divider Circuit:**
  ```
  MQ-135 A0 Output (0V - 5V)
            |
            [ R1 = 10kΩ ]
            |
            +------------> ESP32 GPIO 34 (Analog In, 0V - 3.3V safe)
            |
            [ R2 = 20kΩ ] (or two 10kΩ in series)
            |
           GND
  ```
- **Formula:**
  $$V_{\text{out}} = V_{\text{in}} \times \left( \frac{R_2}{R_1 + R_2} \right) = 5\text{V} \times \left( \frac{20\text{k}\Omega}{10\text{k}\Omega + 20\text{k}\Omega} \right) = 5\text{V} \times \frac{2}{3} \approx 3.33\text{V}$$

---

### B. GPIO 12 (DHT11 Data Pin)
- GPIO 12 is a strapping pin (`MTDI`). If pulled HIGH during power-on boot, the ESP32 sets internal flash memory voltage to 1.8V instead of 3.3V, causing a boot loop.
- Standard DHT11 modules with high-value pull-ups work reliably when powered simultaneously with the ESP32. If boot resets occur, power the board first or set eFuse flash voltage to 3.3V.

---

### C. Why GPIO 34 for Analog Input?
- GPIO 34 is part of **ADC1** (`ADC1_CH6`).
- **Vital Reason:** **ADC2** is shared with the Wi-Fi subsystem and becomes disabled during Wi-Fi transmission. **ADC1** is completely independent of Wi-Fi, allowing accurate analog gas readings while streaming data to the web.

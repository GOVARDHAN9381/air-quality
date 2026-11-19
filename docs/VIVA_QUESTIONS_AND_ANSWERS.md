# MPMC T-3 Viva Voce Examination Guide
## ESP32-Based Real-Time Indoor Air Quality Monitoring, Environmental Sensing and Intelligent Warning System

This guide prepares you to answer every technical and conceptual question your professor or external viva examiner might ask.

---

### Category 1: Architecture & Microcontroller (ESP32)

#### Q1. Why did you choose the ESP32 instead of an Arduino Uno or 8051 for this project?
**Answer:**
1. **Built-in Wi-Fi and Bluetooth:** The ESP32 features integrated 2.4 GHz Wi-Fi (802.11 b/g/n), allowing direct web server hosting and cloud telemetry without needing bulky external shields like ESP8266 or Ethernet shields.
2. **Superior Processing Power:** ESP32 is powered by a dual-core Xtensa 32-bit LX6 microprocessor running at 240 MHz (compared to Uno's 8-bit ATmega328P at 16 MHz).
3. **High-Resolution ADC:** ESP32 provides a 12-bit ADC (resolving 4096 discrete quantization levels from 0 to 4095), whereas Arduino Uno only has a 10-bit ADC (1024 levels).
4. **Flexible Pin Matrix:** ESP32 allows runtime remapping of internal hardware peripherals (like I2C SDA/SCL) to virtually any GPIO pin.

#### Q2. Why is GPIO 34 specifically chosen for the MQ-135 sensor instead of other analog pins?
**Answer:**
ESP32 has two separate analog-to-digital converter blocks: **ADC1** and **ADC2**.
- **ADC2** is internally shared with the Wi-Fi RF baseband. Whenever Wi-Fi is actively scanning, transmitting, or connected, ADC2 channels become inaccessible and return invalid readings (`ESP_ERR_TIMEOUT`).
- **ADC1** (which includes GPIO 32 through 39) operates completely independently of the Wi-Fi subsystem. GPIO 34 is on ADC1 channel 6 (`ADC1_CH6`).
- In addition, GPIO 34 is an **input-only pin** (no internal pull-up/pull-down or output drivers), resulting in the lowest electrical noise for sensitive analog gas readings.

#### Q3. What is the significance of GPIO 12 on the ESP32, and why must caution be taken with DHT11?
**Answer:**
GPIO 12 is a **strapping / bootstrap pin** (named `MTDI`).
During reset or power-up boot, the ESP32 hardware samples the voltage on GPIO 12:
- If GPIO 12 is **HIGH** at boot, the internal LDO voltage regulator for the external SPI flash memory defaults to **1.8V**.
- If GPIO 12 is **LOW** (default), the flash memory regulator outputs **3.3V**.
Since our ESP32 DevKit board uses a standard 3.3V flash memory chip, if the DHT11's pull-up resistor forces GPIO 12 HIGH during boot, the flash memory undervolt occurs and the ESP32 fails to start. Once booted, however, GPIO 12 acts as a completely normal general-purpose digital input/output.

#### Q4. Why did you connect the OLED display to GPIO 22 and 23 instead of the default I2C pins (GPIO 21 and 22)?
**Answer:**
By default in Arduino ESP32 core, I2C SDA is on GPIO 21 and SCL is on GPIO 22.
However, in our circuit design, **GPIO 21 is allocated to drive the Active Buzzer**.
Thanks to the ESP32's internal GPIO multiplexer (GPIO Matrix), we remapped the I2C peripheral to custom pins by calling:
```cpp
Wire.begin(22, 23); // SDA = GPIO 22, SCL = GPIO 23
```
This isolates the digital acoustic alarm from the high-speed I2C serial clock bus.

---

### Category 2: Sensors & Electronics (MQ-135 & DHT11)

#### Q5. Why is a resistor voltage divider mandatory on the MQ-135 output before connecting to GPIO 34?
**Answer:**
- The MQ-135 gas sensor requires a **5V supply** to power its internal tin dioxide ($\text{SnO}_2$) micro-heater to ~200°C. Therefore, its analog output voltage can swing up to $4.5\text{V} - 5.0\text{V}$ under concentrated gas or smoke.
- The ESP32's GPIO pins are fabricated on a 3.3V CMOS process and can **only tolerate a maximum input voltage of 3.3V**. Supplying 5V directly to GPIO 34 would forward-bias the internal ESD protection diodes and permanently burn out the ADC channel.
- We used a voltage divider with $R_1 = 10\text{k}\Omega$ and $R_2 = 20\text{k}\Omega$:
  $$V_{\text{out}} = V_{\text{in}} \times \left( \frac{R_2}{R_1 + R_2} \right) = 5\text{V} \times \frac{20}{30} \approx 3.33\text{V}$$
  This safely steps down the maximum 5V signal to the allowable 3.3V range.

#### Q6. What is the working principle of the MQ-135 gas sensor?
**Answer:**
The sensitive material in the MQ-135 is **Tin Dioxide ($\text{SnO}_2$)**, which has lower electrical conductivity in clean air.
When target reducing or oxidizable gases (such as $\text{CO}_2$, $\text{NH}_3$, benzene, alcohol, or smoke) come into contact with the heated sensor surface:
1. Gas molecules react with oxygen ions adsorbed onto the tin-dioxide crystal boundaries.
2. Electrons are released back into the conduction band.
3. The surface resistance ($R_s$) of the sensor decreases proportionally to the gas concentration.
4. An internal load resistor ($R_L$) converts this change in resistance into a variable output voltage: higher gas concentration produces a higher analog output voltage.

#### Q7. Why does the MQ-135 require a warm-up period?
**Answer:**
The chemical oxidation and reduction reactions on the $\text{SnO}_2$ semiconductor surface require a constant operating temperature of approximately 150°C to 200°C. When first powered on, the internal heater coil needs 20 to 60 seconds to reach thermodynamic equilibrium. For high-precision laboratory calibration, a preheating period of 24–48 hours is recommended. In our firmware and web dashboard, we provide an automated 45-second warm-up countdown timer during which baseline stabilization is maintained.

#### Q8. How does the DHT11 sensor communicate with the ESP32?
**Answer:**
The DHT11 uses a proprietary **single-wire, bidirectional serial communication protocol**:
1. The ESP32 initiates communication by pulling the data bus LOW for at least 18 milliseconds, then pulls it HIGH for 20–40 microseconds.
2. The DHT11 detects this start signal, pulls the bus LOW for 80 microseconds, and then HIGH for 80 microseconds as an acknowledge response.
3. The DHT11 then transmits **40 bits (5 bytes)** of data:
   - Byte 1: Integral Relative Humidity
   - Byte 2: Decimal Relative Humidity
   - Byte 3: Integral Temperature
   - Byte 4: Decimal Temperature
   - Byte 5: Checksum (Sum of Bytes 1 to 4)
4. The ESP32 verifies the checksum byte to guarantee data integrity before displaying the values.

---

### Category 3: The 3 Project Modules & Software Architecture

#### Q9. Explain the 3 main modules of your project as presented in your MPMC PPT.
**Answer:**
1. **Module 1 — Indoor Air Quality & Environmental Sensing:**
   - Physical layer comprising the MQ-135 and DHT11 sensors connected to ESP32.
   - Continuously measures atmospheric pollutants, temperature, and relative humidity.
2. **Module 2 — Real-Time Data Acquisition & Processing:**
   - The ESP32 executes moving-average filtering over 16 analog samples on GPIO 34 to filter ADC noise.
   - Compares raw ADC readings against calibrated thresholds (`thresholdWarning = 1300`, `thresholdAlert = 2100`).
   - Identifies the operating condition state: `NORMAL`, `WARNING`, or `ALERT`.
3. **Module 3 — Intelligent Air Quality Alert & Visualization:**
   - Multi-tier alert generation:
     - Local visual feedback: SSD1306 0.96" OLED screen and status LEDs (Green = Normal, Red = Warning/Alert).
     - Local acoustic feedback: Active buzzer pulsing during `ALERT`.
     - Remote web visualization: Real-time HTML5/Chart.js dashboard showing telemetry gauges, trends, and alarms.

#### Q10. What are your project's warning and critical threshold values, and what happens at each stage?
**Answer:**
- **Stage 1: Normal Condition ($\text{ADC} < 1300$):**
  - Green LED (GPIO 18) = **ON**
  - Red LED (GPIO 19) = **OFF**
  - Buzzer (GPIO 21) = **SILENT**
  - OLED Display = Displays "NORMAL" with live PPM, Temp, and Humidity.
  - Web Dashboard = Green banner "AIR QUALITY STATUS: NORMAL".
- **Stage 2: Warning Condition ($1300 \le \text{ADC} < 2100$):**
  - Green LED = **OFF**
  - Red LED = **STEADY ON**
  - Buzzer = **SILENT** (warns occupants visually without causing acoustic disturbance)
  - Web Dashboard = Amber/Yellow warning banner prompting room ventilation.
- **Stage 3: Critical Alert Condition ($\text{ADC} \ge 2100$):**
  - Green LED = **OFF**
  - Red LED = **FLASHING** (pulsed every 300 ms)
  - Buzzer = **SOUNDING** (pulsed alarm siren)
  - Web Dashboard = Flashing red hazard banner with synthesized audio alarm.

---

### Category 4: Web Visualization & Cloud Deployment

#### Q11. How does the ESP32 transmit its analog values to the web dashboard?
**Answer:**
We implemented two complementary methods:
1. **Direct On-Board Web Server (REST API):**
   The ESP32 runs an asynchronous HTTP server on port 80. Every 1.5–2 seconds, the client web browser issues an asynchronous `fetch('/api/data')` request. The ESP32 packages the analog ADC, calculated PPM, voltage, temperature, humidity, and actuator states into a structured JSON string and replies with HTTP 200.
2. **Cloud HTTP Push:**
   In `esp32_cloud_uploader.ino`, the ESP32 acts as an HTTP client, issuing `HTTP POST /api/telemetry` payloads to a cloud backend (hosted on Render.com or local Node.js server), which broadcasts the telemetry in real time via WebSockets (`socket.io`).

#### Q12. What happens if there is no internet connection in the examination hall?
**Answer:**
Our firmware is designed with a **Dual-Mode Wi-Fi Architecture (`WIFI_AP_STA`)**:
- First, it attempts to connect to the configured Wi-Fi router.
- If the router is unavailable, the ESP32 automatically activates its **SoftAP (Software Access Point)** mode broadcasting a Wi-Fi hotspot named `AirSense-ESP32-Hotspot` (IP: `192.168.4.1`).
- The examiner or student can simply connect their laptop or smartphone directly to the ESP32 hotspot and open `http://192.168.4.1` in any browser to see the live dashboard!
- In addition, the frontend includes an interactive **Demo Mode** to simulate gas spikes and alarm sequences on demand.

#### Q13. How is the analog ADC value converted to PPM (Parts Per Million) in code?
**Answer:**
The MQ-135 has an empirical log-log sensitivity curve relating gas concentration (PPM) to the sensor resistance ratio ($R_s / R_0$).
In our embedded software, the 12-bit ADC reading ($0 - 4095$) corresponds to:
$$V_{\text{measured}} = \left(\frac{\text{ADC}}{4095}\right) \times 3.3\text{V}$$
We approximate the power-law characteristic curve:
$$\text{PPM} = \text{Baseline} + \left(\frac{\text{ADC}}{600}\right)^{2.5} \times 85$$
Where clean atmospheric air is anchored at approximately $400\text{ PPM}$ ($\text{CO}_2$ equivalent).

---

### Category 5: Limitations & Future Enhancements

#### Q14. What are the engineering limitations of this setup?
**Answer:**
1. **Cross-Sensitivity of MQ-135:** The MQ-135 responds to multiple gases ($\text{CO}_2$, $\text{NH}_3$, alcohol, benzene, smoke). It cannot distinguish which specific gas caused an increase without chromatography or multi-sensor array signature analysis.
2. **Temperature & Humidity Drift:** Electrochemical surface conductivity varies with ambient temperature and humidity. Professional units apply real-time compensation polynomials using the DHT11 readings.
3. **DHT11 Sampling Limit:** The DHT11 cannot be sampled faster than once every 1–2 seconds.

#### Q15. How would you enhance this project for commercial or industrial deployment?
**Answer:**
1. **Actuated Ventilation Control:** Connect an opto-isolated 5V relay module to GPIO 25 to automatically trigger an exhaust fan when the warning threshold is reached.
2. **Non-Dispersive Infrared (NDIR) Sensor:** Upgrade from chemical resistance to an optical NDIR sensor (e.g. SenseAir S8 or MH-Z19B) for laboratory-grade $\text{CO}_2$ measurement.
3. **Mobile Push Notifications:** Integrate Telegram Bot API or Pushover to send instant smartphone alerts when harmful gases are detected.
4. **Energy Harvesting & Deep Sleep:** Transition ESP32 into `ESP_DEEP_SLEEP` with ULP (Ultra-Low Power) co-processor polling every 30 seconds for battery-powered operation.

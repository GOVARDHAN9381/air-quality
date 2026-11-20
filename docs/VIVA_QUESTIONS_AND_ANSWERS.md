# MPMC T-3 Viva Voce Examination Guide
## ESP32-Based Real-Time Indoor Air Quality Monitoring, Environmental Sensing and Intelligent Warning System
**Hardware Configuration: DHT11 (GPIO 12) + MQ-135 (GPIO 34) + Active Buzzer (GPIO 21) + Green LED (GPIO 18) + Red LED (GPIO 19) + OLED (GPIO 26/25)**

---

### Category 1: Architecture & Microcontroller (ESP32)

#### Q1. Why did you choose the ESP32 instead of an Arduino Uno or 8051 for this project?
**Answer:**
1. **Integrated Wi-Fi & Networking:** The ESP32 includes on-chip 2.4 GHz Wi-Fi (802.11 b/g/n), allowing direct web server hosting and cloud telemetry without needing external shields or modules.
2. **Dual-Core Processing:** Powered by an Xtensa 32-bit LX6 dual-core processor running at 240 MHz (compared to 8-bit ATmega328P at 16 MHz).
3. **High-Resolution ADC:** ESP32 provides a 12-bit ADC (resolving 4096 discrete levels from 0 to 4095), whereas Arduino Uno only has a 10-bit ADC (1024 levels).
4. **Rich Memory:** 520 KB internal SRAM allows hosting responsive web pages and processing moving-average data buffers without memory exhaustion.

#### Q2. Why is GPIO 34 specifically chosen for the MQ-135 sensor?
**Answer:**
ESP32 has two separate analog-to-digital converter blocks: **ADC1** and **ADC2**.
- **ADC2** is internally shared with the Wi-Fi RF baseband. Whenever Wi-Fi is actively scanning, transmitting, or connected, ADC2 channels become disabled and return invalid readings (`ESP_ERR_TIMEOUT`).
- **ADC1** (which includes GPIO 34) operates completely independently of the Wi-Fi subsystem. GPIO 34 is on ADC1 channel 6 (`ADC1_CH6`).
- In addition, GPIO 34 is an **input-only pin** (no internal pull-up/pull-down or output drivers), resulting in the lowest electrical noise for sensitive analog gas readings.

#### Q3. What is the significance of GPIO 12 on the ESP32, and why must caution be taken with DHT11?
**Answer:**
GPIO 12 is a **strapping / bootstrap pin** (named `MTDI`).
During reset or power-up boot, the ESP32 hardware samples the voltage on GPIO 12:
- If GPIO 12 is **HIGH** at boot, the internal LDO voltage regulator for the external SPI flash memory defaults to **1.8V**.
- If GPIO 12 is **LOW** (default), the flash memory regulator outputs **3.3V**.
Since standard ESP32 DevKit boards use 3.3V SPI flash memory chips, if an external circuit pulls GPIO 12 HIGH during boot, flash undervolt occurs and the ESP32 fails to start. Once booted, however, GPIO 12 operates as a completely standard digital input for DHT11 single-wire communication.

#### Q4. Why did you connect the OLED display to GPIO 26 and 25 instead of the default I2C pins (GPIO 21 and 22)?
**Answer:**
By default in Arduino ESP32 core, I2C SDA is on GPIO 21 and SCL is on GPIO 22.
However, in our circuit design, **GPIO 21 is allocated to drive the Active Buzzer**.
Thanks to the ESP32's internal GPIO multiplexer (GPIO Matrix), we remapped the I2C peripheral to custom pins by calling:
```cpp
Wire.begin(26, 25); // SDA = GPIO 26, SCL = GPIO 25
```
This isolates the digital acoustic alarm from the high-speed I2C serial clock bus. In addition, our firmware includes graceful hardware detection: if the OLED is disconnected, the ESP32 continues running smoothly without hanging!

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
The sensitive material in the MQ-135 is **Tin Dioxide ($\text{SnO}_2$)**, which has low electrical conductivity in clean air.
When target reducing or oxidizable gases (such as $\text{CO}_2$, $\text{NH}_3$, benzene, alcohol, or smoke) come into contact with the heated sensor surface:
1. Gas molecules react with oxygen ions adsorbed onto the tin-dioxide crystal boundaries.
2. Electrons are released back into the conduction band.
3. The surface resistance ($R_s$) of the sensor decreases proportionally to the gas concentration.
4. An internal load resistor converts this resistance drop into a variable output voltage: higher gas concentration produces a higher analog output voltage.

#### Q7. Why does the MQ-135 require a warm-up period?
**Answer:**
The chemical oxidation and reduction reactions on the $\text{SnO}_2$ semiconductor surface require a constant operating temperature of approximately 150°C to 200°C. When first powered on, the internal heater coil needs 20 to 60 seconds to reach thermodynamic equilibrium. In our firmware and web dashboard, we provide an automated 45-second warm-up countdown timer during which baseline stabilization is maintained.

#### Q8. How does the DHT11 sensor communicate with the ESP32?
**Answer:**
The DHT11 uses a proprietary **single-wire, bidirectional serial protocol**:
1. The ESP32 initiates communication by pulling GPIO 12 LOW for at least 18 milliseconds, then pulls it HIGH for 20–40 microseconds.
2. The DHT11 responds by pulling the bus LOW for 80 microseconds, and then HIGH for 80 microseconds.
3. The DHT11 then transmits **40 bits (5 bytes)** of data:
   - Byte 1: Integral Relative Humidity
   - Byte 2: Decimal Relative Humidity
   - Byte 3: Integral Temperature
   - Byte 4: Decimal Temperature
   - Byte 5: Checksum (Sum of Bytes 1 to 4)
4. The ESP32 verifies the checksum byte to guarantee data integrity before updating variables.

---

### Category 3: The 3 Project Modules & Software Architecture

#### Q9. Explain the 3 main modules of your project as presented in your MPMC PPT.
**Answer:**
1. **Module 1 — Indoor Air Quality & Environmental Sensing:**
   - Physical layer comprising the MQ-135 on GPIO 34 (analog gas) and DHT11 on GPIO 12 (digital temp/humidity).
2. **Module 2 — Real-Time Data Acquisition & Processing:**
   - The ESP32 executes moving-average filtering over 16 analog samples on GPIO 34 to filter ADC noise.
   - Compares raw ADC readings against calibrated thresholds (`thresholdWarning = 1300`, `thresholdAlert = 2100`).
   - Identifies the operating condition state: `NORMAL`, `WARNING`, or `ALERT`.
3. **Module 3 — Intelligent Air Quality Alert & Visualization:**
   - Local on-board feedback: OLED display, Green LED (Safe on GPIO 18), Red LED (Warning/Alert on GPIO 19), and Active Buzzer (Acoustic Siren on GPIO 21).
   - Remote IoT feedback: Web Dashboard plotting real-time charts and sounding browser alarms.

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
  - Buzzer = **SOUNDING ALARM** (pulsed acoustic siren)
  - Web Dashboard = Flashing red hazard banner with synthesized audio siren.

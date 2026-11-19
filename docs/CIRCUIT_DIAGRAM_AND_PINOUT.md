# Hardware Circuit Connections, Pinout & Electrical Design
**Project: ESP32-Based Real-Time Indoor Air Quality Monitoring System**
**Course Reference: MPMC T-3 Presentation**

---

## 1. Complete Pinout Matrix

| Component | Component Pin | ESP32 DevKit Pin | Signal Type | Electrical Notes & Precautions |
| :--- | :--- | :--- | :--- | :--- |
| **MQ-135 Gas Sensor** | **A0 (Analog Out)** | **GPIO 34** | Analog Input (ADC1_CH6) | **Input-Only Pin**. Must use a **resistor voltage divider** to step 5V output down to &le; 3.3V. |
| **MQ-135 Gas Sensor** | **VCC** | **VIN / 5V Rail** | Power (5V DC) | Internal heating element ($\text{SnO}_2$) requires 5V to achieve operating temperature (~150°C - 200°C). |
| **MQ-135 Gas Sensor** | **GND** | **GND** | Ground | Common system ground. |
| **DHT11 Sensor** | **Data (D)** | **GPIO 12** | Digital 1-Wire Single Bus | Needs pull-up (typically 4.7kΩ to 10kΩ, built into breakout module). *See Strapping Pin note below.* |
| **DHT11 Sensor** | **VCC** | **3.3V** | Power (3.3V DC) | Connect to ESP32 3.3V regulated output pin. |
| **DHT11 Sensor** | **GND** | **GND** | Ground | Common system ground. |
| **0.96" OLED (SSD1306)**| **SDA (Data)** | **GPIO 22** | I2C Serial Data | Assigned custom I2C data wire in firmware via `Wire.begin(22, 23)`. |
| **0.96" OLED (SSD1306)**| **SCL (Clock)** | **GPIO 23** | I2C Serial Clock | Assigned custom I2C clock wire in firmware via `Wire.begin(22, 23)`. |
| **0.96" OLED (SSD1306)**| **VCC** | **3.3V** | Power (3.3V DC) | Operating voltage is 3.3V. |
| **0.96" OLED (SSD1306)**| **GND** | **GND** | Ground | Common system ground. |
| **Green LED (Normal)** | **Anode (+)** | **GPIO 18** | Digital Output (Active HIGH)| In series with a **220Ω or 330Ω** current limiting resistor. |
| **Green LED (Normal)** | **Cathode (-)** | **GND** | Ground | Common ground. |
| **Red LED (Warning)** | **Anode (+)** | **GPIO 19** | Digital Output (Active HIGH)| In series with a **220Ω or 330Ω** current limiting resistor. |
| **Red LED (Warning)** | **Cathode (-)** | **GND** | Ground | Common ground. |
| **Active Buzzer** | **Positive (+)** | **GPIO 21** | Digital Output (Active HIGH)| 5V or 3.3V active buzzer. Sounds audible alert when pulsed HIGH. |
| **Active Buzzer** | **Negative (-)** | **GND** | Ground | Common ground. |

---

## 2. Critical Engineering Considerations (Frequently Asked in Viva)

### A. The MQ-135 Voltage Divider on GPIO 34
- **The Problem:** The MQ-135 gas sensor's heater coil is powered by **5V**. Consequently, when high gas concentrations are detected, its analog output voltage ($A_0$) can swing from $0\text{V}$ up to nearly $4.5\text{V} - 5.0\text{V}$.
- **The Danger:** ESP32 ADC inputs (like GPIO 34) are strictly rated for a maximum of **3.3V**. Applying 5V directly to GPIO 34 will cause electrical overstress and permanently destroy the ADC input channel.
- **The Solution (Voltage Divider):**
  Connect a simple two-resistor voltage divider between MQ-135 $A_0$ and ESP32 GPIO 34:
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
  This safely scales the maximum $5\text{V}$ sensor output down to $3.33\text{V}$, perfectly matching ESP32's 12-bit ADC range ($0 - 4095$).

---

### B. GPIO 12 Strapping Pin & Boot Behaviour
- **Why is GPIO 12 sensitive?** 
  GPIO 12 (`MTDI`) is an internal **strapping pin** on the ESP32 chip.
  - If GPIO 12 is held **HIGH** during power-on reset (boot), the ESP32 sets the internal flash memory supply voltage (VDD_SDIO) to **1.8V**.
  - If GPIO 12 is held **LOW** (default), the flash memory operates at **3.3V**.
  - Most standard ESP32 DevKit boards use 3.3V SPI flash memory chips. Therefore, if GPIO 12 is pulled HIGH by an external circuit at the exact moment of power-on, the chip will fail to boot from flash memory and enter a boot loop (`rst:0x10 (RTCWDT_RTC_RESET)`).
- **How our project handles it:**
  The DHT11 sensor data pin typically has a weak pull-up resistor. To ensure clean booting:
  1. If you experience boot resets when plugging in the USB cable, disconnect the DHT11 data line during boot, or burn the ESP32 eFuse to lock flash voltage to 3.3V:
     ```bash
     espefuse.py set_flash_voltage 3.3V
     ```
  2. Alternatively, ensure the board is powered after connection. Once booted, GPIO 12 functions as a completely standard, high-speed bidirectional digital GPIO.

---

### C. Custom I2C Pin Assignment (GPIO 22 & GPIO 23)
- The default hardware I2C pins on the ESP32 are:
  - Default SDA: GPIO 21
  - Default SCL: GPIO 22
- **Why did we reassign them?**
  In our circuit, **GPIO 21 is occupied by the Active Buzzer**. 
- ESP32 features a fully reconfigurable GPIO matrix. Any digital pin can be routed to the internal hardware I2C peripheral. In our firmware:
  ```cpp
  Wire.begin(22, 23); // SDA = GPIO 22, SCL = GPIO 23
  ```
  This frees up GPIO 21 for the high-priority buzzer alarm while providing 400 kHz fast I2C communication to the SSD1306 OLED display.

---

### D. Why GPIO 34 for Analog Input?
- GPIO 34 belongs to **ADC1** (`ADC1_CH6`).
- **Vital Reason:** The ESP32 has two ADCs: ADC1 and ADC2.
  - **ADC2** is shared with the Wi-Fi subsystem. Whenever Wi-Fi is active (transmitting packets or connected to an AP), ADC2 cannot be read and returns corrupted readings.
  - **ADC1** (which includes GPIO 34, 35, 36, 39) operates completely independently of the Wi-Fi transceiver.
  - Furthermore, GPIO 34 is an **input-only pin** without internal pull-ups or output drivers, making it the most stable, low-noise analog input channel on the entire ESP32.

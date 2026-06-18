# 🌤️ Smart Weather Station Project Using (PIC16F877A)
# Group 26 CoE Mostafa Elnady,Mohamed Kahir , Sara Samadi 
# 64190039,64190013,64190022

![C](https://img.shields.io/badge/Language-C99-blue.svg)
![Microcontroller](https://img.shields.io/badge/MCU-PIC16F877A-red.svg)
![Compiler](https://img.shields.io/badge/Compiler-XC8_v3.10-orange.svg)
![Status](https://img.shields.io/badge/Status-Fully_Operational-success.svg)

> Our Project is real-time environmental monitoring system built from the scratch , Features I2C display , hardware RTC timekeeping, analog-to-digital sensor processing,data logging

---

## ⚡ Project Features

* **Real-Time Environmental Tracking:**  humidity and temperature reading via (DHT22)
* **Analog Light & Heat Sensing:** 10-bit ADC processing for ambient light levels (LDR) and secondary analog temperature cross-checking (LM35)
* **Hardware Timekeeping:** Precision I2C communication with a DS3231 RTC module to timestamp all recorded events
* **Non-Volatile Data Logging:** Implements a cooperative circular buffer utilizing the DS3231's internal SRAM to save historical telemetry data across power cycles.
* **Live Telemetry Stream:** Streams live metrics out via UART (9600 Baud) to serial monitor
* **Thermal Alert System:**  hardware interrupt/threshold system triggers a warning LED if temperatures exceed safe limits (23.0°C)

---

## 🛠️ Hardware We used

### Components List
* **Microcontroller:** Microchip PIC16F877A (@ 20MHz External Crystal)
* **Display:** 16x2 Character LCD I2C
* **Sensors:** DHT22 (Digital), LM35 (Analog), LDR (Analog)
* **Clock:** DS3231 I2C Real-Time Clock

### Pinout Mapping done on Protues and the Breadboard
| Component | PIC16 Pin | Port / Function | Protocol / Type |
| :--- | :--- | :--- | :--- |
| **I2C SDA** | Pin 23 | RC4 | I2C Data Line |
| **I2C SCL** | Pin 18 | RC3 | I2C Clock Line |
| **DHT22** | Pin 19 | RD0 | Custom Digital Pulse |
| **LM35** | Pin 3 | RA1 (AN1) | Analog Voltage Input |
| **LDR** | Pin 4 | RA2 (AN2) | Analog Voltage Input |
| **Alert LED** | Pin 39 | RB6 | Digital Output |



---

## 💻 Software used

* **IDE:** MPLAB X IDE (v6.20+)
* **Compiler:** XC8 (v3.10) - Strict C99 Standard
* **Flasher:** Microchip PICkit 3
* **UART SERIALING:** Aurduino IDE


### Serial Output


```text
# Smart Weather Station v3 (I2C LCD + DHT22 + DS3231)
Last 8 saved samples:
time,date,temp,hum,light,alert
14:30:00,28/05/26,24.5,45,71,0
14:30:02,28/05/26,24.5,45,72,0
--- End of saved data ---

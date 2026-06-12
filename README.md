# ⚡ Smart Microgrid Monitoring & Control System (ESP32)

A real-time Smart Microgrid Energy Management System (EMS) built using ESP32 that monitors solar generation, environmental conditions, and battery status while automatically switching between solar and battery power sources to ensure uninterrupted operation of critical loads.

## 🚀 Features

- Real-time Solar Voltage, Current, and Power Monitoring
- Battery State of Charge (SOC) Monitoring
- Temperature, Humidity, and Light Intensity Monitoring
- Automatic Solar ↔ Battery Power Switching
- Relay-Based Load Control
- Live ESP32 Web Dashboard
- Energy Generation Analytics and Forecasting
- Event Logging and System Status Monitoring
- Offline Access via ESP32 WiFi Server

## ⚙️ Hardware Used

- ESP32 Development Board
- ACS712 Current Sensor
- DHT11 Temperature & Humidity Sensor
- LDR Sensor
- Voltage Divider Circuit
- 2-Channel Relay Module
- Solar Panel
- Battery
- LED Load
- DC Fan Load

## 🔄 System Workflow

1. ESP32 acquires real-time sensor data.
2. Solar voltage, current, power, and environmental parameters are calculated.
3. Energy Management System (EMS) evaluates solar availability.
4. Loads are powered by solar when generation is sufficient.
5. Loads automatically switch to battery during low solar conditions.
6. Dashboard displays live system status, power flow, and energy metrics.

## ☀️ Power Management Logic

| Condition | Power Source | Action |
|------------|-------------|---------|
| High Solar Generation | Solar | Loads powered from solar |
| Low Solar Generation | Battery | Automatic source switching |
| Night Time | Battery | Backup operation |

## 📊 Dashboard Features

- Solar Power Monitoring
- Battery Status Visualization
- Environmental Monitoring
- Energy Forecasting
- Load Status Tracking
- Historical Power Trends
- Event Logs

## 🛠️ Technologies Used

- ESP32
- Arduino IDE
- Embedded C/C++
- HTML, CSS, JavaScript
- Chart.js
- WiFi Web Server

## 🎯 Applications

- Smart Homes
- Solar Hybrid Systems
- Renewable Energy Monitoring
- Remote Power Systems
- Educational Smart Grid Demonstrations

## 🔮 Future Improvements

- MPPT-Based Charging
- Cloud Connectivity
- Mobile Application
- AI-Based Energy Forecasting
- Load Prioritization
- Smart Grid Integration

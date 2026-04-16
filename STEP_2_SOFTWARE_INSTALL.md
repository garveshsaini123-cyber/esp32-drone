# 📖 SkyPilot Drone — Complete Setup Guide

> Everything you need: from unboxing parts to your first flight.
> Follow these steps in order. Don't skip ahead!

---

## 📋 Guide Overview

| Step | Title | What You'll Do | Time |
|------|-------|----------------|------|
| **1** | [🔌 Hardware Setup](STEP_1_HARDWARE_SETUP.md) | Wire ESP32, MPU6050, ESCs, motors | 1–2 hours |
| **2** | [💻 Software Install](STEP_2_SOFTWARE_INSTALL.md) | Install ESP-IDF, Python, drivers | 30–60 min |
| **3** | [🔨 Build & Flash](STEP_3_BUILD_AND_FLASH.md) | Compile firmware, upload to ESP32 | 15–30 min |
| **4** | [📱 Phone Setup](STEP_4_PHONE_SETUP.md) | Connect phone, use web controller | 5–10 min |
| **5** | [🚁 First Flight](STEP_5_FIRST_FLIGHT.md) | Test, tune PID, and fly! | 2–4 hours |

---

## ⚡ Quick Start (If You Know What You're Doing)

```powershell
# 1. Open ESP-IDF PowerShell
# 2. Navigate to project
cd "C:\Users\Aarav Aditya\Desktop\drone code"

# 3. Build
idf.py set-target esp32
idf.py build

# 4. Flash (replace COM3 with your port)
idf.py -p COM3 flash monitor

# 5. On phone: connect to WiFi "SkyPilot-Drone" (password: drone1234)
# 6. Open http://192.168.4.1 in browser
# 7. Fly! (after proper testing phases)
```

---

## 🔧 Troubleshooting Quick Reference

| Problem | Solution | Guide Section |
|---------|----------|---------------|
| MPU6050 not found | Check SDA/SCL wires, AD0→GND | [Step 1](STEP_1_HARDWARE_SETUP.md) |
| ESP-IDF not recognized | Reopen ESP-IDF PowerShell | [Step 2](STEP_2_SOFTWARE_INSTALL.md) |
| Flash fails | Hold BOOT button during flash | [Step 3](STEP_3_BUILD_AND_FLASH.md) |
| Phone can't find WiFi | Wait 10s after ESP32 boot | [Step 4](STEP_4_PHONE_SETUP.md) |
| Phone switches to mobile data | Airplane mode + WiFi on | [Step 4](STEP_4_PHONE_SETUP.md) |
| Controller page won't load | Try `http://192.168.4.1` not https | [Step 4](STEP_4_PHONE_SETUP.md) |
| Motors don't spin | Check ESC wiring + arming sequence | [Step 5](STEP_5_FIRST_FLIGHT.md) |
| Drone wobbles | Reduce PID Kp gains | [Step 5](STEP_5_FIRST_FLIGHT.md) |
| Drone flips on takeoff | Wrong motor direction or prop placement | [Step 5](STEP_5_FIRST_FLIGHT.md) |

---

## 📁 Project File Map

```
drone code/
├── 📖 README.md                    ← Project overview
├── 📖 guide/                       ← YOU ARE HERE
│   ├── GUIDE.md                    ← This index file
│   ├── STEP_1_HARDWARE_SETUP.md    ← Wiring & parts
│   ├── STEP_2_SOFTWARE_INSTALL.md  ← ESP-IDF installation
│   ├── STEP_3_BUILD_AND_FLASH.md   ← Compile & upload
│   ├── STEP_4_PHONE_SETUP.md       ← Phone controller
│   └── STEP_5_FIRST_FLIGHT.md      ← Testing & flying
├── ⚙️ main/                        ← Firmware source code
│   ├── config.h                    ← All configuration
│   ├── main.c                      ← Boot sequence
│   ├── imu_sensor.c/h              ← MPU6050 driver
│   ├── pid_controller.c/h          ← PID algorithm
│   ├── motor_control.c/h           ← ESC/motor PWM
│   ├── wifi_controller.c/h         ← WiFi + web server
│   ├── flight_controller.c/h       ← Flight loop
│   ├── battery_monitor.c/h         ← Battery ADC
│   └── CMakeLists.txt              ← Build file list
├── 📱 phone_app/
│   └── phone_controller.html       ← Standalone phone app
├── CMakeLists.txt                  ← Top-level build
└── sdkconfig.defaults              ← ESP32 settings
```

---

## 🆘 Need Help?

- **ESP-IDF Docs**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/
- **MPU6050 Datasheet**: https://invensense.tdk.com/products/motion-tracking/6-axis/mpu-6050/
- **ESP32 Pinout**: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/

---

**Start here → [Step 1: Hardware Setup](STEP_1_HARDWARE_SETUP.md)**

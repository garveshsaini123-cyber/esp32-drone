# 🔨 Step 3 — Build & Flash the Firmware

> Compile the SkyPilot firmware and upload it to your ESP32.

---

## 3.1 — Open ESP-IDF Terminal

1. Click **Start Menu**
2. Search for **"ESP-IDF PowerShell"** (or "ESP-IDF CMD")
3. Open it — this terminal has all the ESP-IDF tools ready

---

## 3.2 — Navigate to the Project

```powershell
cd "C:\Users\Aarav Aditya\Desktop\drone code"
```

Verify you're in the right folder:
```powershell
dir
```
You should see:
```
    CMakeLists.txt
    README.md
    sdkconfig.defaults
    main/
    phone_app/
    guide/
```

---

## 3.3 — Set the Target Chip

Tell ESP-IDF that we're building for ESP32:

```powershell
idf.py set-target esp32
```

This creates a `build/` folder and `sdkconfig` file. Takes about 30 seconds.

**Expected output:**
```
-- Configuring done
-- Generating done
-- Build files have been written to: .../build
```

---

## 3.4 — Build the Firmware

```powershell
idf.py build
```

⏱️ **First build takes 3–5 minutes.** Subsequent builds are much faster.

**Expected output (at the end):**
```
[100%] Built target skypilot_drone.elf
Project build complete. To flash, run this command:
  idf.py -p (PORT) flash
```

### ❌ If the Build Fails

Common errors and fixes:

| Error | Fix |
|-------|-----|
| `CMake Error: Could not find...` | Run `set-target` again |
| `fatal error: esp_wifi.h: No such file` | ESP-IDF not installed properly, reinstall |
| `undefined reference to...` | Check `main/CMakeLists.txt` has all .c files listed |
| `multiple definition of pid_t` | Rename `pid_t` to `drone_pid_t` in config.h if conflict with system type |

---

## 3.5 — Connect ESP32 & Find COM Port

1. Plug your ESP32 into your computer via USB cable
2. Open **Device Manager** (Win+X → Device Manager)
3. Under **"Ports (COM & LPT)"**, find your ESP32
4. Note the COM port (e.g., `COM3`, `COM5`, `COM7`)

---

## 3.6 — Flash the Firmware

Replace `COM3` with YOUR actual COM port:

```powershell
idf.py -p COM3 flash
```

**Expected output:**
```
Connecting.........
Chip is ESP32-D0WD
...
Writing at 0x00010000... (  7%)
Writing at 0x00014000... ( 14%)
...
Writing at 0x00078000... (100%)
Hard resetting via RTS pin...
```

### ❌ If Flash Fails

| Problem | Fix |
|---------|-----|
| "Failed to connect" | Hold the **BOOT** button on ESP32 while flashing |
| "No serial port" | Install CP2102 or CH340 driver (see Step 2) |
| "Permission denied" | Close Arduino IDE or any other serial monitor |
| "Wrong COM port" | Check Device Manager for correct port |

**The BOOT button trick:**
1. Run the flash command
2. When you see "Connecting.........", **press and hold the BOOT button** on the ESP32
3. Release after you see "Writing at..."

---

## 3.7 — Monitor Serial Output

After flashing, immediately open the serial monitor to see if everything works:

```powershell
idf.py -p COM3 monitor
```

**You should see:**
```
╔══════════════════════════════════════════════╗
║                                              ║
║   ▲ SkyPilot Flight Controller v1.0.0       ║
║   ESP32 Quadcopter Firmware                  ║
║                                              ║
║   WiFi: SkyPilot-Drone                       ║
║   Control: http://192.168.4.1                ║
║                                              ║
╚══════════════════════════════════════════════╝

[1/7] NVS Flash initialized ✓
[2/7] Scanning I2C bus...
┌─────────────────────────────────┐
│   I2C Bus Scanner               │
│   SDA=GPIO21  SCL=GPIO22        │
└─────────────────────────────────┘
  ✓ Device found at address 0x68 (104)
    └── MPU6050 / MPU6500 / MPU9250
  Total: 1 device(s) found on I2C bus.
[3/7] IMU sensor initialized ✓
[4/7] Motor PWM channels initialized ✓
[5/7] WiFi AP started ✓
[6/7] Battery monitor started ✓
[7/7] Flight controller started ✓

╔══════════════════════════════════════════════╗
║   ✓ ALL SYSTEMS INITIALIZED                  ║
║                                              ║
║   1. Connect phone to WiFi: SkyPilot-Drone   ║
║   2. Open: http://192.168.4.1                ║
║   3. Use joysticks to control!               ║
╚══════════════════════════════════════════════╝
```

### 🔍 What to Check in the Output

1. **I2C Scanner**: Did it find the MPU6050?
   - ✅ `Device found at 0x68` → Sensor working!
   - ❌ `NO I2C devices found` → Check wiring (see Step 1)
   - ⚠️ `Device found at 0x69` → Change `MPU6050_ADDR` to `0x69` in `config.h`

2. **All 7 steps show ✓?** → You're ready for the phone!

3. **IMU calibration**: You'll see "Calibrating..." logs. Keep the drone **flat and still** during this!

### 📌 Keyboard Shortcuts in Monitor

| Key | Action |
|-----|--------|
| `Ctrl+]` | Exit the monitor |
| `Ctrl+T` → `Ctrl+R` | Reset the ESP32 |

---

## 3.8 — Build + Flash + Monitor (All-in-One)

For convenience, you can do all three at once:

```powershell
idf.py -p COM3 flash monitor
```

---

## ✅ Build & Flash Checklist

- [ ] `idf.py set-target esp32` completed
- [ ] `idf.py build` shows 100% success
- [ ] ESP32 flashed via correct COM port
- [ ] Serial monitor shows boot banner
- [ ] I2C scanner found MPU6050 at 0x68
- [ ] All 7 initialization steps show ✓
- [ ] WiFi AP "SkyPilot-Drone" visible on phone

---

**→ Next: [Step 4 — Phone Controller Setup](STEP_4_PHONE_SETUP.md)**

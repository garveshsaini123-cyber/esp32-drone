# 📱 Step 4 — Phone Controller Setup

> Your phone IS the remote control. No extra transmitter needed.
> Just connect to the drone's WiFi and open the web controller.

---

## 4.1 — Power On the Drone

1. Make sure **NO PROPELLERS ARE INSTALLED** (safety first!)
2. Connect the ESP32 via USB **or** connect the LiPo battery
3. Wait 5–10 seconds for the ESP32 to boot
4. The built-in blue LED should start blinking slowly (= IDLE state)

---

## 4.2 — Connect Your Phone to the Drone's WiFi

The ESP32 creates its own WiFi hotspot. Your phone connects directly to it.

### On Android:

1. Open **Settings** → **WiFi**
2. Find the network: **`SkyPilot-Drone`**
3. Tap on it
4. Enter password: **`drone1234`**
5. **IMPORTANT**: You may see a message saying *"This network has no internet access"*
   - Tap **"Stay connected"** or **"Connect anyway"**
   - If your phone keeps switching back to mobile data:
     - Go to **Settings → WiFi → SkyPilot-Drone → ⚙️**
     - Disable **"Auto-switch to mobile data"**
     - Or turn on **Airplane Mode** first, then turn WiFi back on

### On iPhone:

1. Open **Settings** → **Wi-Fi**
2. Find: **`SkyPilot-Drone`**
3. Tap and enter password: **`drone1234`**
4. If it says "No Internet Connection" → that's normal, stay connected
5. You might need to tap **"Use Without Internet"** if prompted

---

## 4.3 — Open the Controller

### Option A: Built-in Web Controller (Easiest) ⭐

1. Open your phone's **web browser** (Chrome recommended)
2. Type in the address bar: **`http://192.168.4.1`**
3. The controller page will load directly from the ESP32!
4. **Rotate your phone to LANDSCAPE mode** (horizontal)

### Option B: Local HTML File

If the web server isn't working, you can use the local file:

1. Transfer `phone_app/phone_controller.html` to your phone
   - Via USB cable, WhatsApp to yourself, email, etc.
2. Open the HTML file in Chrome:
   - Type in Chrome address bar: `file:///sdcard/Download/phone_controller.html`
   - Or use a file manager app to open with Chrome
3. It will automatically try to connect via WebSocket

---

## 4.4 — Controller Interface Guide

Here's what you see on the phone screen:

```
┌────────────────────────────────────────────────────────────┐
│  ▲ SKYPILOT                          [LINKED] [SAFE]      │
│                                                            │
│           THR: 0   ROLL: 0   PITCH: 0   YAW: 0           │
│                                                            │
│                                                            │
│    ┌───────────┐                      ┌───────────┐       │
│    │           │                      │           │       │
│    │     ●     │    (Left Stick)      │     ●     │       │
│    │  Throttle │                      │   Pitch   │       │
│    │    Yaw    │                      │    Roll   │       │
│    └───────────┘                      └───────────┘       │
│                                                            │
│              [ARM]   [LAND]   [CALIBRATE]                 │
└────────────────────────────────────────────────────────────┘
```

### Top Bar (HUD)
| Element | Meaning |
|---------|---------|
| **LINKED** (green) | ✅ Phone is connected and sending data |
| **NO LINK** (red) | ❌ Not connected — check WiFi |
| **SAFE** (gray) | Motors are disarmed (safe) |
| **ARMED** (red, blinking) | ⚠️ Motors are LIVE! |

### Left Joystick
| Direction | Action |
|-----------|--------|
| **Up** | Increase throttle (climb) |
| **Down** | Decrease throttle (descend) |
| **Left** | Yaw left (rotate CCW) |
| **Right** | Yaw right (rotate CW) |

### Right Joystick
| Direction | Action |
|-----------|--------|
| **Up** | Pitch forward (fly forward) |
| **Down** | Pitch backward (fly backward) |
| **Left** | Roll left (strafe left) |
| **Right** | Roll right (strafe right) |

### Buttons
| Button | Action | When to use |
|--------|--------|-------------|
| **ARM** | Enables the motors | Before flying. Throttle must be at zero! |
| **DISARM** | Kills all motors instantly | Emergency stop or after landing |
| **LAND** | Auto-landing sequence | Drone gradually reduces throttle |
| **CALIBRATE** | Re-calibrate the IMU sensor | Only when disarmed, drone must be flat & still |

---

## 4.5 — Connection Status Indicators

The phone controller shows real-time connection status:

| Status | Badge Color | Meaning |
|--------|-------------|---------|
| Connected | 🟢 Green "LINKED" | All good, packets flowing |
| Disconnected | 🔴 Red "NO LINK" | WiFi lost, auto-reconnecting... |

**The controller auto-reconnects** every 2 seconds if the connection drops.

---

## 4.6 — Telemetry Display

The numbers at the top update in real-time (50 times/second):

| Value | Range | Meaning |
|-------|-------|---------|
| **THR** | 0–2000 | Throttle level |
| **ROLL** | -128 to +127 | Left/Right tilt command |
| **PITCH** | -128 to +127 | Forward/Backward tilt command |
| **YAW** | -128 to +127 | Rotation speed command |

---

## 4.7 — Android Quick-Switch Fix

Android phones sometimes disconnect from WiFi networks that don't have internet.

### Permanent Fix:
1. Connect to `SkyPilot-Drone`
2. Go to **Settings → WiFi**
3. **Long-press** on `SkyPilot-Drone`
4. Tap **Modify network** or the ⚙️ gear icon
5. Look for any of these options and **disable** them:
   - "Auto switch to mobile data"
   - "Smart network switch"
   - "Detect internet availability"
6. Save

### Alternative Fix:
1. Turn on **Airplane Mode** ✈️
2. Turn WiFi back on (it will stay on even in airplane mode)
3. Connect to `SkyPilot-Drone`
4. Now it won't switch because there's no mobile data to fall back to

---

## 4.8 — Screen Lock Prevention

The controller uses the **Wake Lock API** to prevent your phone screen from turning off while flying. If your screen still dims:

1. Go to **Settings → Display → Screen timeout**
2. Set it to **10 minutes** or **Never** (for the flight session)
3. Remember to change it back later!

---

## ✅ Phone Setup Checklist

- [ ] ESP32 is powered on and booted
- [ ] Phone connected to WiFi: `SkyPilot-Drone` with password `drone1234`
- [ ] Phone stays on drone WiFi (doesn't switch to mobile data)
- [ ] Browser opened to `http://192.168.4.1`
- [ ] Controller page loaded in landscape mode
- [ ] Status shows "LINKED" (green badge)
- [ ] Joysticks respond when you touch them
- [ ] Telemetry values change when you move sticks

---

**→ Next: [Step 5 — First Flight](STEP_5_FIRST_FLIGHT.md)**

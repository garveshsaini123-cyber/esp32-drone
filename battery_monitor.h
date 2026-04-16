# 🚁 Step 5 — First Flight (Testing & Flying)

> This is where everything comes together.
> Follow this EXACTLY to avoid damage or injury.

---

## ⚠️ SAFETY RULES — READ BEFORE DOING ANYTHING

```
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║   1. REMOVE ALL PROPELLERS for the first 3 test phases       ║
║   2. NEVER put your fingers near spinning motors             ║
║   3. Always have a way to cut power (unplug battery)         ║
║   4. Test OUTDOORS in an open area (away from people)        ║
║   5. Keep a FIRE EXTINGUISHER nearby (LiPo batteries!)       ║
║   6. NEVER fly over people, roads, or animals                ║
║   7. Check propeller tightness EVERY flight                  ║
║   8. NEVER charge LiPo batteries unattended                  ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
```

---

## Phase 1: Bench Test WITHOUT Propellers ⭐

**Goal**: Verify all systems work before any motors spin.

### 1.1 — Power up and verify serial output

1. **Remove ALL propellers** (I'll keep reminding you 😄)
2. Connect ESP32 via USB
3. Open serial monitor: `idf.py -p COM3 monitor`
4. Verify all 7 init steps pass ✓
5. Verify MPU6050 found at 0x68

### 1.2 — Test phone connection

1. Connect phone to `SkyPilot-Drone` WiFi
2. Open `http://192.168.4.1`
3. Verify "LINKED" badge appears
4. Move joysticks — check serial monitor shows changing values:
   ```
   [IDLE] R:0.0° P:0.0° Y:0.0° | T:0 A:0 | dt:4.0ms
   ```

### 1.3 — Test IMU sensor

1. Pick up the ESP32+MPU6050 and tilt it slowly
2. Watch the serial monitor — roll and pitch angles should change:
   ```
   [IDLE] R:15.3° P:-8.2° Y:2.1° | T:0 A:0
   ```
3. Tilt left → Roll should go positive
4. Tilt forward → Pitch should go positive
5. Place it flat → Both should return near 0

**If angles don't change** → IMU wiring issue or calibration needed.
**If angles are wrong axis** → Sensor is rotated. You may need to swap X/Y in code.

### 1.4 — Test arming

1. Make sure **throttle is at ZERO** (left stick centered/down)
2. Press **ARM** on the phone
3. Serial monitor should show:
   ```
   ╔══════════════════════════════════╗
   ║   ⚡ MOTORS ARMED! CAREFUL! ⚡   ║
   ╚══════════════════════════════════╝
   ```
4. Phone badge changes to 🔴 **ARMED**
5. Press **DISARM** — motors should stop, badge goes back to gray

---

## Phase 2: Motor Direction Test (Still NO Propellers!)

**Goal**: Verify all 4 motors spin in the correct direction.

### 2.1 — Arm and apply low throttle

1. Keep propellers OFF!
2. If using battery: connect LiPo now
3. ARM the drone from phone
4. **Very slowly** push the **left stick UP** (throttle ~200-300)
5. All 4 motors should start spinning!

### 2.2 — Check motor directions

Look at each motor from above:

| Motor | Position | Should Spin |
|-------|----------|-------------|
| M1 (GPIO 25) | Front-Left | **Counter-Clockwise** ↺ |
| M2 (GPIO 26) | Front-Right | **Clockwise** ↻ |
| M3 (GPIO 27) | Rear-Left | **Clockwise** ↻ |
| M4 (GPIO 14) | Rear-Right | **Counter-Clockwise** ↺ |

### 2.3 — Fix wrong motor direction

If a motor spins the wrong way:
- **Option A**: Swap any 2 of the 3 motor wires going into the ESC
- **Option B**: Use BLHeli configurator to reverse the motor in software

### 2.4 — Check motor response to sticks

While armed at low throttle:
- Push **right stick RIGHT** → M1 & M3 speed up, M2 & M4 slow down (= roll right)
- Push **right stick FORWARD** → M1 & M2 speed up, M3 & M4 slow down (= pitch forward)
- Push **left stick RIGHT** → Yaw right

If any direction is reversed, you may need to swap motor wires or adjust signs in `motor_control.c`.

**DISARM when done!**

---

## Phase 3: Tethered / Strapped Test

**Goal**: Test PID stabilization with propellers, but drone is restrained.

### 3.1 — Install propellers

NOW you can install the propellers:
- **CW props** on M2 (front-right) and M3 (rear-left)
- **CCW props** on M1 (front-left) and M4 (rear-right)
- Make sure they're tight!

### 3.2 — Secure the drone

- **Method A**: Strap the drone to a heavy book/board with zip ties
- **Method B**: Put it in a large cardboard box (sides protect you)
- **Method C**: Hold it down with a heavy weight on the center

### 3.3 — Test stabilization

1. Place the drone on a flat surface
2. ARM from phone
3. **Slowly** increase throttle until you feel the drone pulling up slightly
4. Watch serial monitor — PID should be correcting:
   ```
   [FLYING] R:2.1° P:-1.3° Y:0.5° | T:1200 A:1
   ```
5. Tilt the platform slightly — the motors should adjust to compensate
6. **DISARM** when done

### 3.4 — If the drone oscillates wildly

This means PID gains are too high. Reduce them in `config.h`:

```c
/* Try these conservative values first */
#define PID_ROLL_KP   0.6f    /* Was 1.2, cut in half */
#define PID_ROLL_KI   0.01f   /* Was 0.02, cut in half */
#define PID_ROLL_KD   0.3f    /* Was 0.5, reduce */
```

Rebuild and reflash: `idf.py -p COM3 flash monitor`

---

## Phase 4: First Free Flight 🚁

**Goal**: Actual flight! Do this OUTDOORS in a large open area.

### Pre-Flight Checklist:

- [ ] Large open area (at least 10m × 10m), no people nearby
- [ ] Battery fully charged (12.4V+ for 3S)
- [ ] All propellers tight and correct direction
- [ ] Phone fully charged
- [ ] Wind is calm (< 10 km/h)
- [ ] First-aid kit nearby (just in case)

### 4.1 — The First Hover

1. Place drone on flat ground
2. Stand at least **3 meters back**
3. Connect phone to drone WiFi
4. Open controller in landscape mode
5. Press **CALIBRATE** (drone must be flat and still for 5 seconds)
6. Wait for calibration to complete
7. Press **ARM**
8. **SLOWLY** push throttle up until the drone lifts ~30cm off the ground
9. Try to keep it steady with **tiny** stick movements
10. To land: **slowly** bring throttle down OR press **LAND**
11. Press **DISARM** after landing

### 4.2 — Flying Tips

| Tip | Why |
|-----|-----|
| Make **tiny** stick movements | Big movements = crashes |
| Keep altitude low at first (< 1 meter) | Less damage if things go wrong |
| If losing control → **DISARM immediately** | Better to drop from 1m than crash into something |
| Practice hover before moving | Hovering is the hardest part |
| Always know where the **DISARM button** is | Your emergency stop |
| Land with at least 20% battery | LiPo batteries die quickly at low voltage |

### Emergency Procedures:

| Situation | Action |
|-----------|--------|
| Drone flying away | Press **DISARM** immediately |
| Uncontrollable wobble | Press **DISARM** and reduce PID gains |
| Phone disconnects | Failsafe kicks in after 2 seconds → motors cut |
| Battery alarm | Press **LAND** or reduce throttle and land manually |
| Crash | **Unplug battery immediately**, check for damage |

---

## Phase 5: PID Tuning

**Goal**: Make the flight smooth and stable.

### Basic Tuning Process:

1. **Start with all gains very low:**
   ```c
   Kp = 0.5,  Ki = 0.0,  Kd = 0.0
   ```

2. **Increase Kp** until the drone barely oscillates when tilted:
   ```c
   Kp = 0.5 → 0.8 → 1.0 → 1.2 → 1.5
   ```
   - Stop when you see slight oscillation
   - Back down slightly from that point

3. **Add Kd** to dampen the oscillation:
   ```c
   Kd = 0.1 → 0.2 → 0.3 → 0.5
   ```
   - This should make it feel "smooth"

4. **Add tiny Ki** to correct slow drift:
   ```c
   Ki = 0.005 → 0.01 → 0.02
   ```
   - Too much Ki = slow oscillation

5. **Repeat for each axis** (Roll, Pitch, Yaw)

### After every change:
```powershell
idf.py -p COM3 flash monitor
```

---

## ✅ Flight Checklist

- [ ] Phase 1: Bench test passed (no props)
- [ ] Phase 2: Motor directions verified (no props)
- [ ] Phase 3: Tethered test with props — stable
- [ ] Phase 4: First hover achieved!
- [ ] Phase 5: PID gains tuned for smooth flight

---

## 🎉 Congratulations!

You've built and flown your own phone-controlled drone!

**Next steps to explore:**
- Add GPS module (NEO-6M) for position hold
- Add barometer (BMP280) for altitude hold
- Add LED strips for night flying
- Add a camera (ESP32-CAM) for FPV
- Build a custom Android app with more features

---

**← Back to: [README](../README.md)**

# 📦 Step 1 — Hardware Setup & Wiring

> Before touching any code, get your hardware assembled and wired correctly.
> This is the most critical step. Wrong wiring = fried components.

---

## 🛒 Parts List (What You Need)

| # | Component | Specification | Qty | Approx. Cost (₹) |
|---|-----------|---------------|-----|-------------------|
| 1 | **ESP32 DevKit V1** | 30-pin or 38-pin module | 1 | ₹400–500 |
| 2 | **MPU6050 Module** | GY-521 breakout board | 1 | ₹100–150 |
| 3 | **Brushless Motors** | 2205 or 2207, 2300KV | 4 | ₹600–800 each |
| 4 | **ESCs** | 30A BLHeli_S | 4 | ₹300–500 each |
| 5 | **Propellers** | 5045 or 6045 (2 CW + 2 CCW) | 2 sets | ₹100–200 |
| 6 | **LiPo Battery** | 3S 11.1V 1500–2200mAh | 1 | ₹1000–1500 |
| 7 | **Frame** | 250mm quadcopter frame | 1 | ₹300–500 |
| 8 | **XT60 Connector** | For battery connection | 1 | ₹50 |
| 9 | **Jumper Wires** | Male-to-female, male-to-male | 1 pack | ₹80 |
| 10 | **Resistors** | 100kΩ + 10kΩ (battery divider) | 1 each | ₹5 |
| 11 | **Pull-up Resistors** | 4.7kΩ (for I2C, if needed) | 2 | ₹5 |
| 12 | **USB Cable** | Micro-USB (for ESP32 programming) | 1 | ₹100 |

**Optional but recommended:**
- Buzzer (piezo, for alarms)
- LiPo charger (B6AC or similar)
- LiPo safe bag
- Heat shrink tubing
- Zip ties & double-sided tape

---

## 🔌 Wiring Diagram

### ESP32 to MPU6050 (I2C Connection)

```
ESP32                    MPU6050 (GY-521)
┌──────────┐            ┌──────────────┐
│          │            │              │
│  GPIO 21 ├────────────┤ SDA          │
│          │            │              │
│  GPIO 22 ├────────────┤ SCL          │
│          │            │              │
│    3.3V  ├────────────┤ VCC          │
│          │            │              │
│    GND   ├────────────┤ GND          │
│          │            │              │
│          │      GND ──┤ AD0          │  ← Connect AD0 to GND!
│          │            │              │
└──────────┘            └──────────────┘
```

> ⚠️ **IMPORTANT**: Connect the **AD0** pin on the MPU6050 to **GND**.
> This sets the I2C address to `0x68`. If AD0 is floating or HIGH, the
> address becomes `0x69` and the code won't find the sensor.

### ESP32 to ESCs (PWM Motor Control)

```
ESP32                    ESCs
┌──────────┐
│          │            ┌─── ESC 1 (Front-Left) ──── Motor 1
│  GPIO 25 ├────────────┤ Signal wire (white/yellow)
│          │            └─── GND (black)
│          │
│          │            ┌─── ESC 2 (Front-Right) ─── Motor 2
│  GPIO 26 ├────────────┤ Signal wire
│          │            └─── GND
│          │
│          │            ┌─── ESC 3 (Rear-Left) ───── Motor 3
│  GPIO 27 ├────────────┤ Signal wire
│          │            └─── GND
│          │
│          │            ┌─── ESC 4 (Rear-Right) ──── Motor 4
│  GPIO 14 ├────────────┤ Signal wire
│          │            └─── GND
│          │
└──────────┘

⚡ ESC power wires (red + black thick wires) connect to the LiPo battery
   through a power distribution board or directly.
```

### Battery Voltage Monitor (Optional but Recommended)

```
Battery (+) ──[100kΩ]──┬──[10kΩ]── GND
                       │
                   GPIO 34 (ADC)
                       │
                    ESP32
```

> This voltage divider lets the ESP32 safely read an 11.1V–12.6V battery
> through its 3.3V-max ADC pin. Without it, you'll fry the ESP32!

### Motor Position (Looking Down at the Drone)

```
        ↑ FRONT ↑
        
    M1 (CCW)    M2 (CW)
    GPIO 25     GPIO 26
        \      /
         \    /
          \  /
           \/
           /\
          /  \
         /    \
        /      \
    M3 (CW)     M4 (CCW)
    GPIO 27     GPIO 14
    
        ↓ REAR ↓
```

> **CW** = Clockwise propeller, **CCW** = Counter-Clockwise propeller.
> Using the wrong prop direction will make the drone uncontrollable!

---

## ✅ Wiring Checklist

Before moving to the next step, verify every connection:

- [ ] MPU6050 SDA → GPIO 21
- [ ] MPU6050 SCL → GPIO 22
- [ ] MPU6050 VCC → ESP32 3.3V
- [ ] MPU6050 GND → ESP32 GND
- [ ] MPU6050 AD0 → GND ← **Don't forget this!**
- [ ] ESC 1 signal → GPIO 25
- [ ] ESC 2 signal → GPIO 26
- [ ] ESC 3 signal → GPIO 27
- [ ] ESC 4 signal → GPIO 14
- [ ] ESC ground wires → ESP32 GND (common ground!)
- [ ] Battery divider resistors → GPIO 34
- [ ] Buzzer → GPIO 13 (optional)
- [ ] **NO PROPELLERS INSTALLED YET!** ← Critical safety step

---

## 📸 Quick Test (Before Coding)

1. Plug in the ESP32 via USB
2. Check the blue LED blinks (shows it's getting power)
3. **Do NOT connect the LiPo battery yet** — just use USB power for now
4. The MPU6050 should have a red LED lit when powered via 3.3V

If the MPU6050 LED doesn't light up → check VCC/GND wires.

---

**→ Next: [Step 2 — Software Installation](STEP_2_SOFTWARE_INSTALL.md)**

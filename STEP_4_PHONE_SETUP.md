# 💻 Step 2 — Software Installation

> Install everything needed to compile and flash the firmware to your ESP32.

---

## 2.1 — Install Required Software

### A) Install Python (Required by ESP-IDF)

1. Go to: **https://www.python.org/downloads/**
2. Download **Python 3.11+** (latest stable)
3. During install, **CHECK** ✅ "Add Python to PATH"
4. Click "Install Now"
5. Verify it works:
   ```
   python --version
   ```
   You should see something like `Python 3.11.x`

---

### B) Install Git

1. Go to: **https://git-scm.com/download/win**
2. Download and install with default settings
3. Verify:
   ```
   git --version
   ```

---

### C) Install ESP-IDF (Espressif IoT Development Framework)

This is the official tool to compile and flash code for ESP32.

#### Option 1: ESP-IDF Windows Installer (Easiest) ⭐

1. Go to: **https://dl.espressif.com/dl/esp-idf/**
2. Download the **ESP-IDF Tools Installer** (latest v5.x)
3. Run the installer:
   - Select **ESP-IDF v5.2** or latest v5.x
   - Choose install location (default is fine): `C:\Espressif\`
   - Let it install all tools (CMake, Ninja, toolchain, etc.)
4. After install, you'll see **"ESP-IDF PowerShell"** and **"ESP-IDF Command Prompt"** in your Start Menu

#### Option 2: Manual Install (Advanced)

```powershell
# Clone ESP-IDF
mkdir C:\Espressif
cd C:\Espressif
git clone -b v5.2 --recursive https://github.com/espressif/esp-idf.git

# Run the install script
cd esp-idf
.\install.ps1 esp32

# Set up environment (run this EVERY TIME you open a new terminal)
.\export.ps1
```

---

### D) Verify ESP-IDF Installation

1. Open **"ESP-IDF PowerShell"** from Start Menu
2. Type:
   ```powershell
   idf.py --version
   ```
3. You should see something like:
   ```
   ESP-IDF v5.2
   ```

If this works, you're ready to build! 🎉

---

## 2.2 — Install Serial Driver (if needed)

Most ESP32 DevKit boards use one of these USB-to-serial chips:

| Chip on Board | Driver Download |
|---------------|-----------------|
| **CP2102** | https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers |
| **CH340** | http://www.wch-ic.com/downloads/CH341SER_EXE.html |

**How to check:**
1. Plug in your ESP32 via USB
2. Open **Device Manager** (Win+X → Device Manager)
3. Look under "Ports (COM & LPT)"
4. You should see something like `Silicon Labs CP210x (COM3)` or `CH340 (COM5)`
5. **Note the COM port number** — you'll need it later!

If you see a yellow warning triangle ⚠️, install the driver from the links above.

---

## 2.3 — Install a Code Editor (Optional)

For editing the code, you can use either:

- **VS Code** (recommended): https://code.visualstudio.com/
  - Install the "ESP-IDF" extension for build/flash buttons
  - Install the "C/C++" extension for syntax highlighting

- **Arduino IDE**: If you're more comfortable with Arduino
  - Note: This project uses ESP-IDF, not Arduino. But you can adapt it.

---

## ✅ Installation Checklist

Before moving to the next step, verify:

- [ ] Python 3.11+ installed and in PATH
- [ ] Git installed
- [ ] ESP-IDF v5.x installed
- [ ] `idf.py --version` works in ESP-IDF terminal
- [ ] ESP32 shows up in Device Manager with a COM port
- [ ] COM port number noted: **COM_____**

---

**→ Next: [Step 3 — Build & Flash the Firmware](STEP_3_BUILD_AND_FLASH.md)**

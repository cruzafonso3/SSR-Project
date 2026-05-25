# ESP32 Bluetooth Tools

This ESP32 is a dedicated **Bluetooth reconnaissance and spoofing device**. It scans for BLE and BT Classic devices nearby, and can spoof fake BLE devices.

## What It Does

- **BLE Scanner** — Discovers nearby BLE devices (name, MAC, RSSI)
- **BLE Spoofer** — Advertises as fake BLE devices with realistic appearances
  - Presets: AirPods Pro, Tesla Model 3, Samsung TV
  - Custom name option
- **BT Classic Scan** — Inquiry scan for Bluetooth Classic devices
- **No WiFi** — Pure Bluetooth tool to save flash space

## Hardware

| Component | Pin | Notes |
|-----------|-----|-------|
| OLED SDA | GPIO 21 | I2C Data |
| OLED SCL | GPIO 22 | I2C Clock |
| OLED VCC | 3.3V | **Never 5V** |
| OLED GND | GND | |
| BTN UP | GPIO 14 | Pin ↔ GND (internal pullup) |
| BTN DOWN | GPIO 27 | Pin ↔ GND (internal pullup) |
| BTN SELECT | GPIO 26 | Pin ↔ GND (internal pullup) |

## Libraries

Install via Arduino IDE → Sketch → Include Library → Manage Libraries:
1. **Adafruit SSD1306** by Adafruit
2. **Adafruit GFX Library** by Adafruit (auto-installs with SSD1306)

**Built-in:** `BLEDevice.h`, `BluetoothSerial.h` (already in ESP32 core)

## Upload

1. Open `ESP32_Bluetooth_Tools.ino` in Arduino IDE
2. Select **Tools → Board → ESP32 Dev Module**
3. Select **Tools → Port** (your ESP32 port)
4. Click **Upload**

## How to Use

| Button | Function |
|--------|----------|
| **UP/DOWN** | Move cursor / scroll results |
| **SELECT** | Start scan, start spoof, or go back |

**Menu:**
```
> BLE Scanner   ← Scan for 10 seconds, then show results
> BLE Spoofer   ← Choose preset (AirPods, Tesla, Samsung, Custom)
> BT Classic    ← Scan for BT Classic devices
> Status        ← View RAM, spoof state, counts
> Stop Spoof    ← Stop advertising if active
```

## BLE Spoofer Presets

| Preset | Advertised Name | Appearance |
|--------|-----------------|------------|
| AirPods Pro | `AirPods Pro` | Headset (0x03E0) |
| Tesla Model 3 | `Tesla Model 3` | Generic Phone (0x0080) |
| Samsung TV | `[TV] Samsung QLED` | TV (0x03EC) |
| Custom | `ESP32_Custom` | Generic |

## Testing

Use a phone with a BLE scanner app (like **nRF Connect**) to see the spoofed devices.

## ⚠️ Warning

For authorized educational use only in isolated lab environments.

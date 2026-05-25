# ESP32 Rogue AP

This ESP32 creates a **fake WiFi access point** with a captive portal to harvest credentials. All DNS queries on the rogue network resolve to the ESP32 itself.

## What It Does

- **Rogue Access Point** — Creates an open WiFi network with a configurable SSID
- **DNS Hijack** — Every domain query resolves to `192.168.4.1` (the ESP32)
- **Captive Portal** — Serves a dark-themed WiFi login page to any connected client
- **Credential Logging** — Captured usernames/passwords are printed to Serial Monitor
- **Editable SSID** — Change the fake network name directly on the OLED menu

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

**Built-in:** `WiFi.h`, `WebServer.h` (already in ESP32 core)

## Upload

1. Open `ESP32_Rogue_AP.ino` in Arduino IDE
2. Select **Tools → Board → ESP32 Dev Module**
3. Select **Tools → Port** (your ESP32 port)
4. Click **Upload**

## How to Use

| Button | Function |
|--------|----------|
| **UP/DOWN** | Move cursor |
| **SELECT** | Toggle AP ON/OFF, enter edit mode, or view status |

**Menu:**
```
> Rogue AP   [OFF]  ← Toggle ON to start the fake AP
> Edit SSID        ← Change the network name
> Status >         ← View AP IP, SSID, captured credentials
```

**Default SSID:** `Free_WiFi` (change via Edit SSID menu)

## Capturing Credentials

1. Toggle **Rogue AP** ON
2. Wait for victims to connect to your fake network
3. When they enter credentials on the portal page, check **Serial Monitor** (115200 baud):
   ```
   [PORTAL] CAPTURED: john_doe:password123
   ```

## Testing

Connect a phone/laptop to the fake AP, open any HTTP page, and you'll be redirected to the captive portal login.

## ⚠️ Warning

For authorized educational use only in isolated lab environments.

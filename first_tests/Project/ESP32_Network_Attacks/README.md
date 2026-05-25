# ESP32 Network Attacks

This ESP32 handles **ARP Spoofing, DNS Spoofing, DHCP Spoofing, and Deauthentication Attacks** with an OLED menu interface.

## What It Does

- **ARP Spoofing** — Poisons victim and gateway ARP caches every N ms
- **DNS Spoofing** — Intercepts queries for your target domain and returns a fake IP
- **DHCP Spoofing** — Responds to DHCP Discover with malicious Offer/ACK (sets router/DNS to ESP32)
- **Deauth Attack** — Sends 802.11 deauthentication frames to force disconnections
- **Transparent Forwarding** — Keeps victim online while intercepting traffic

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

## Configuration

**Before uploading**, edit the top of `ESP32_Network_Attacks.ino`:

```cpp
const char* WIFI_SSID     = "YOUR_LAB_SSID";     // Your isolated lab WiFi
const char* WIFI_PASS     = "YOUR_LAB_PASSWORD"; // Your lab WiFi password
uint8_t VICTIM_MAC[6]     = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; // Victim MAC
IPAddress VICTIM_IP(192, 168, 1, 100);                              // Victim IP
uint8_t GATEWAY_MAC[6]    = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}; // Gateway MAC
IPAddress GATEWAY_IP(192, 168, 1, 1);                               // Gateway IP
const char* TARGET_DOMAIN = "example.com";                          // Domain to spoof
IPAddress SPOOFED_IP(192, 168, 1, 200);                             // Fake IP to return
```

**Find MAC addresses:** On a machine connected to the lab network, run `arp -a`

## Upload

1. Open `ESP32_Network_Attacks.ino` in Arduino IDE
2. Select **Tools → Board → ESP32 Dev Module**
3. Select **Tools → Port** (your ESP32 port)
4. Click **Upload**

## How to Use

| Button | Function |
|--------|----------|
| **UP/DOWN** | Move cursor |
| **SELECT** | Toggle attack ON/OFF |

**Menu:**
```
> ARP Spoof  [OFF]  ← Toggle
> DNS Spoof  [OFF]  ← Toggle
> DHCP Spoof [OFF]  ← Toggle
> Deauth     [OFF]  ← Toggle
> Status >          ← View packet counts, RAM, uptime
```

Press **SELECT** on an attack to turn it ON or OFF.

## Testing

On the victim machine:
```bash
# Check ARP cache
arp -a

# Query target domain
nslookup example.com
```

## Wireshark Detection

See the main project `WIRESHARK_DETECTION.md` for filters to identify these attacks.

## ⚠️ Warning

For authorized educational use only in isolated lab environments.

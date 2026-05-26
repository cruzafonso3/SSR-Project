# DHCP Starvation — Pool Exhaustion (ESP32)

Educational DHCP pool exhaustion attack on an ESP32 with OLED display and button controls. Exhausts the DHCP server's address pool by connecting with spoofed MAC addresses, each requesting a new lease.

## Hardware

| Component | Pin |
|-----------|-----|
| OLED SSD1306 SDA | GPIO 21 |
| OLED SSD1306 SCL | GPIO 22 |
| Button UP | GPIO 14 (internal pullup, GND) |
| Button DOWN | GPIO 27 (internal pullup, GND) |
| Button SELECT | GPIO 26 (internal pullup, GND) |

## How It Works

The ESP cycles through random MAC addresses, connecting to the target Wi-Fi and triggering a DHCP request for each one:

1. **Generate random MAC** — Sets the locally-administered bit (`0x02`) for valid random addressing.
2. **Spoof MAC** — Uses `esp_wifi_set_mac()` to change the STA interface MAC.
3. **Reconnect** — Disconnects and reconnects, forcing a new DHCPDISCOVER → DHCPOFFER → DHCPREQUEST → DHCPACK cycle.
4. **Each unique MAC gets a lease** — The DHCP server assigns an IP from its pool, consuming one address per cycle.
5. **Pool exhaustion detection** — When 10+ consecutive reconnects fail to get an IP, the display shows `FULL: YES`, indicating the pool is likely exhausted.

## Menu (OLED + Buttons)

```
  Connect WiFi
  Disconnect WiFi
  Start Starvation     ← enters live attack screen
  Stop Starvation
  Status               ← scrollable: SSID, IP, reconnects, consecutive fails, pool status
  Settings             ← edit SSID, Password, Cooldown
  Save Config
```

### Live Attack Screen

When the attack is running:

```
=== STARVATION ===
RECON: 42
IP: 192.168.1.105
FAIL: 10
FULL: YES (10+ fails)
SEL/BACK: Stop
```

This screen updates with live data. Pressing SELECT or BACK stops the attack. Exiting to the menu leaves the attack running — returning to Status shows live data.

## First Boot

On first flash, NVS is auto-reset. All config fields start empty. Configure via **Settings → SSID / Password / Cooldown**, then **Save Config**.

## Serial CLI (Fallback)

Full command set available over USB serial at 115200 baud.

```
  set_ssid <name>
  set_pass <password>
  set_cooldown <ms>    (500-10000)
  connect / start / stop / status / save / load / reset
```

## Build

```bash
pio run
pio run --target upload
```

## Technical Notes

- The original ESP32 MAC is saved at init and restored on stop.
- Cooldown default: 1000ms (minimum 500ms, maximum 10000ms).
- Each cycle waits up to 5 seconds for a DHCP lease.
- The `ieee80211_raw_frame_sanity_check` bypass is included for low-level frame injection compatibility (not strictly required for this attack).

> **Authorized lab use only.** Do not use on networks you do not own.

# ARP Spoof — Standalone MITM (ESP32)

Educational Man-in-the-Middle attack using ARP cache poisoning on an ESP32 with OLED display and button controls. No PC required after initial flash.

## Hardware

| Component | Pin |
|-----------|-----|
| OLED SSD1306 SDA | GPIO 21 |
| OLED SSD1306 SCL | GPIO 22 |
| Button UP | GPIO 14 (internal pullup, GND) |
| Button DOWN | GPIO 27 (internal pullup, GND) |
| Button SELECT | GPIO 26 (internal pullup, GND) |

## How It Works

The ESP poisons the ARP caches of both a **victim** and the **gateway** so all traffic between them flows through the ESP. The core technique:

1. **Driver bypass** — Overrides `ieee80211_raw_frame_sanity_check()` to return `0`, unlocking raw frame injection that ESP32 normally blocks.
2. **Gratuitous ARP Replies** — Injects broadcast ARP Reply frames claiming the gateway's IP belongs to the ESP's MAC (sent to victim) and vice-versa, at 1-second intervals.
3. **Sticky ARP interception** — When a corrected ARP cache triggers an ARP request, the promiscuous callback intercepts it and fires back an instant spoofed reply, keeping the poison active even with forwarding OFF.

## Menu (OLED + Buttons)

```
  Connect WiFi
  Disconnect WiFi
  Start ARP            ← starts poison + forwarding
  Stop ARP             ← stops both
  Toggle Forwarding    ← stops/resumes data relay independently
  Status               ← scrollable: SSID, IPs, MACs, state, packet stats
  Settings             ← edit SSID, Password, Victim IP/MAC, Gateway IP/MAC, ARP Cooldown
  Save Config
```

- **Forwarding OFF** — ARP poison stays active, no data relay, but sticky ARP interception prevents cache recovery.
- **Forwarding ON** — Packets are captured, inspected, and relayed between victim and gateway.

## First Boot

On first flash, NVS is auto-reset (version hash mismatch). All config fields start empty. Use **Settings → SSID / Password / Victim IP&MAC / Gateway IP&MAC** to configure, then **Save Config**.

Every re-flash wipes NVS automatically so no stale config persists.

## Serial CLI (Fallback)

Full command set available over USB serial at 115200 baud as an alternative to the OLED menu.

## Build

```bash
pio run
pio run --target upload
```

Requires PlatformIO + the Adafruit SSD1306 and GFX libraries (auto-resolved in `platformio.ini`).

## Technical Notes

- Uses `esp_wifi_internal_tx()` for ARP frame injection (raw Ethernet frames).
- Uses `esp_wifi_80211_tx()` for packet forwarding (raw 802.11 frames).
- Promiscuous mode is ref-counted between the ARP engine and the forwarder — enabling either activates it, disabling both shuts it off.
- The duplicate-symbol linker flag `-Wl,--allow-multiple-definition` enables the driver bypass.

> **Authorized lab use only.** Do not use on networks you do not own.

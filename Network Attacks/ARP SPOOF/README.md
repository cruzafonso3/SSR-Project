# ARP Spoofing — Man-in-the-Middle (ESP32)

Educational ARP cache poisoning + MitM tool for the ESP32. Standalone operation with OLED display and button controls — no PC required after initial flash.

## Hardware

| Component | Pin |
|-----------|-----|
| OLED SSD1306 SDA | GPIO 21 |
| OLED SSD1306 SCL | GPIO 22 |
| Button UP | GPIO 14 (internal pullup, GND) |
| Button DOWN | GPIO 27 (internal pullup, GND) |
| Button SELECT | GPIO 26 (internal pullup, GND) |

## How It Works

The ESP poisons the ARP caches of both a **victim** and the **gateway**, routing all traffic between them through the ESP.

### Driver Bypass

The ESP32's closed-source Wi-Fi driver silently drops raw injected frames via `ieee80211_raw_frame_sanity_check()`. This is overridden:

```cpp
extern "C" {
    int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
        return 0;
    }
}
```

Requires linker flag `-Wl,--allow-multiple-definition` in `platformio.ini`.

### ARP Poison

Uses **gratuitous ARP replies** (opcode 2) sent as **broadcast** 802.11 frames to bypass AP client isolation. Two packets sent every `cooldown_ms` (default 1000ms):

| Packet | Sender IP | Sender MAC | Target IP | Effect |
|--------|-----------|------------|-----------|--------|
| 1 | Gateway IP | ESP's MAC | Gateway IP | Victim caches: gateway → ESP |
| 2 | Victim IP | ESP's MAC | Victim IP | Gateway caches: victim → ESP |

### Sticky ARP Interception

When forwarding is OFF, the ESP still runs promiscuous mode (ref-counted with the forwarder). Any ARP request for the victim or gateway IP is intercepted and answered immediately with a spoofed reply — preventing the ARP cache from ever correcting itself.

### Packet Forwarding

When forwarding is ON, the promiscuous callback captures IPv4 traffic between victim and gateway, re-encapsulates it in a new 802.11 frame, and transmits it via `esp_wifi_80211_tx()`. Both the ARP engine and the forwarder share a promiscuous mode ref-counter — enabling either activates it, disabling both shuts it off.

## OLED Menu

```
  Connect WiFi
  Disconnect WiFi
  Start ARP            ← poison + forwarding ON
  Stop ARP             ← both OFF
  Toggle Forwarding    ← stops/resumes data relay, poison stays
  Status               ← scrollable: SSID, IPs, MACs, ARP/Fwd state, packet stats
  Settings             ← edit SSID, Password, Victim IP/MAC, Gateway IP/MAC, ARP Cooldown
  Save Config
```

### Settings Editors

| Setting | Editor Type | Description |
|---------|-------------|-------------|
| SSID / Password | String editor | UP/DN scroll charset, SEL add, [SPC] [DEL] [END] |
| Victim IP / Gateway IP | IP editor | UP/DN change octet, SEL next octet, auto-save |
| Victim MAC / Gateway MAC | MAC editor | Same as IP editor, hex bytes |
| ARP Cooldown | Int editor | 100-10000ms, step 100 |

## First Boot

On first flash, the build version hash (`__DATE__ __TIME__`) is saved to NVS. Every reflash changes the hash, triggering an automatic factory reset — no stale config survives.

## Serial CLI (Fallback)

Full command set available over USB serial at 115200 baud alongside the OLED interface.

## Build

```bash
cd "Network Attacks/ARP SPOOF"
pio run
pio run --target upload
```

## Key Technical Details

- ARP frames sent via `esp_wifi_internal_tx(WIFI_IF_STA, ...)` — raw Ethernet frames
- Forwarded packets sent via `esp_wifi_80211_tx(WIFI_IF_STA, ...)` — raw 802.11 frames
- Promiscuous mode ref-counted between ARP engine and packet forwarder
- `esp_wifi_set_mac()` not used — the ESP operates with its real MAC
- Default credentials and all target/gateway info must be configured before starting

> **Authorized lab use only.** Do not use on networks you do not own.

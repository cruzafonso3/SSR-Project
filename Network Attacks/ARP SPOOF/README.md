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

## Performing the Attack

### Step 1 — Find target info on the laptop

On the laptop you want to intercept (the **victim**), find its IP, its MAC, the gateway IP, and the gateway MAC:

```bash
ip addr show              # look for your IP under your Wi-Fi interface
ip link show              # look for your MAC address
ip route show default     # this is your gateway IP
iw dev wlan0 link         # the "Connected to" line is the gateway BSSID/MAC
```

### Step 2 — Configure the ESP via OLED

| Menu path | What to set |
|-----------|-------------|
| **Settings → SSID** | The Wi-Fi network name the ESP should connect to |
| **Settings → Password** | The Wi-Fi password |
| **Settings → Victim IP** | The laptop's IP (from Step 1) |
| **Settings → Victim MAC** | The laptop's MAC (from Step 1) |
| **Settings → Gateway IP** | The gateway's IP (from Step 1) |
| **Settings → Gateway MAC** | The gateway's BSSID (from Step 1) |
| **Save Config** | Persist all values to NVS |

### Step 3 — Connect to Wi-Fi

Select **Connect WiFi** from the main menu. The OLED shows "Connecting..." and then "WiFi connected" with the assigned IP.

### Step 4 — Start the attack

Select **Start ARP**. The ESP now:
- Sends ARP poison packets every cooldown interval
- Enables promiscuous mode to intercept ARP requests (sticky poisoning)
- Enables packet forwarding (data relay)

### Step 5 — Verify on the laptop

Check the ARP cache:

```bash
ip neigh show
```

The gateway IP should now point to the **ESP's MAC address**, not the real gateway's MAC. Example:

```
192.168.1.1 dev wlp3s0 lladdr a4:f0:0f:5c:66:c0 REACHABLE
```

### Step 6 — Observe traffic

On the ESP's **Status** page, the packet counters increment as traffic passes through:

```
C: 100  F: 95  D: 5
```

Cap = captured, F = forwarded, D = dropped.

### Optional — Toggle forwarding

Select **Toggle Forwarding** to stop data relay while keeping ARP poison active. Pings stop reaching the internet immediately. The sticky ARP interception prevents cache recovery. Select it again to resume forwarding.

### Using serial CLI instead of OLED

Connect via serial monitor (115200 baud) and use the same commands:

```
set_ssid MyNetwork
set_pass MyPassword
set_target_ip 192.168.1.100
set_target_mac aa:bb:cc:dd:ee:ff
set_gateway_ip 192.168.1.1
set_gateway_mac 11:22:33:44:55:66
save
connect
start
status
```

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

# ARP Spoofing MitM — Success Story & Technical Deep-Dive

> **Project:** Educational Man-in-the-Middle (MitM) Demonstration via ARP Spoofing  
> **Hardware:** ESP32 DOIT DevKit V1, Asus ROG Strix Laptop, BQ Aquaris X2 / Raspberry Pi 5  
> **Date:** 2026-05-22  
> **Status:** ✅ **SUCCESS** — ARP Spoofing fully functional with driver bypass and correct frame formatting.

---

## 1. The Journey: From Failure to Success

This project began as an attempt to demonstrate ARP spoofing on an ESP32. What followed was an exhaustive debugging process that uncovered hidden driver limitations, incorrect protocol implementations, and Access Point security features. This document explains every failure, every fix, and the final working architecture.

---

## 2. Initial Failures: Why ARP Spoofing Didn't Work

### Failure 1: The Hidden Driver Filter
The ESP32's Wi-Fi driver contains a closed-source function called `ieee80211_raw_frame_sanity_check()`. When the ESP32 operates as a Wi-Fi Station (STA) connected to an Access Point, this function **silently discards** all raw 802.11 frames sent via `esp_wifi_80211_tx()`.

**The deceptive part:** `esp_wifi_80211_tx()` returns `ESP_OK` (success), making it appear as if the frame was transmitted. In reality, the driver drops it internally before it ever reaches the radio.

**Evidence:**
- `test_udp` worked (normal IP traffic path is open)
- `test_raw` showed nothing in Wireshark
- `ip neigh show` never changed
- `esp_wifi_80211_tx` returned `ESP_OK` but frames never appeared on air

### Failure 2: Wrong ARP Frame Format
Our initial code sent ARP **Requests** (opcode 1) instead of ARP **Replies** (opcode 2). Linux and most operating systems ignore unsolicited ARP Requests for cache updates. Only ARP Replies force the OS to update its mapping.

### Failure 3: Unicast Delivery Blocked by AP Security
Smartphone hotspots (Android/iOS) implement **Client Isolation** — they drop STA-to-STA unicast data frames. Our ARP packets were addressed directly to the laptop's MAC, so the Access Point filtered them to prevent network abuse.

Additionally, modern hotspots have **IP Spoofing Detection** and **ARP Inspection**:
- The AP detects that a STA (ESP32) is sending frames claiming the AP's IP address
- The AP knows the real gateway MAC and rejects contradictory claims
- This is a deliberate security feature to prevent ARP poisoning attacks

### Failure 4: ESP-IDF Rewrite Didn't Help
We rewrote the entire project in ESP-IDF (pure C) to bypass the Arduino wrapper. The result was identical — `test_raw` still failed. This proved the restriction was **below the framework layer**, in the closed-source Wi-Fi binary blob itself.

### Failure 5: Raspberry Pi 5 AP (First Attempt)
We tested with a Pi 5 running `hostapd` with `ap_isolate=0`. `test_raw` still failed. Why? Because we had **not yet discovered the driver bypass**. The Pi 5 was not the problem; the ESP32 driver's internal filter was.

---

## 3. The Breakthrough: Discovering the Driver Bypass

### The Key Discovery
A community-known workaround exists: overriding the `ieee80211_raw_frame_sanity_check()` function to always return `0` (success). This bypasses the driver's internal filter.

### The Bypass Code
```cpp
#include <stdint.h>

// Bypass ESP32 Wi-Fi driver raw frame sanity check filter
// This allows esp_wifi_80211_tx() to inject raw 802.11 frames
extern "C" {
    int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
        return 0; // Bypass the block filter
    }
}
```

Because this function exists in the ESP32's closed-source `libnet80211.a` library, the linker sees a duplicate symbol. We added a linker flag to allow the override:
```ini
build_flags = 
    -Wl,--allow-multiple-definition
```

### Verification: Deauth Attack Works
With the bypass active, we tested deauthentication (deauth) frames — a type of 802.11 **management** frame. The laptop immediately disconnected from the network. This was definitive proof that **raw injection was unlocked**.

---

## 4. Fixing the Protocol: From Request to Reply

Even with the bypass, the ARP cache on the laptop refused to update. We had to fix the frame format:

### What Changed

| Parameter | Before (Broken) | After (Working) |
|-----------|----------------|-----------------|
| **ARP Opcode** | 1 (Request) | 2 (Reply) |
| **Delivery** | Unicast to target MAC | Broadcast `FF:FF:FF:FF:FF:FF` |
| **Sender IP** | Gateway IP | Gateway IP (for gateway poison) |
| **Target IP** | Target IP | Gateway IP (gratuitous format) |
| **Target MAC** | Target MAC | `00:00:00:00:00:00` (gratuitous) |
| **en_sys_seq** | `true` | `false` |

### Why Broadcast 802.11 + Gratuitous ARP Works
1. **Broadcast 802.11:** The Access Point sees `addr3 = FF:FF:FF:FF:FF:FF` and **floods** the frame to ALL connected stations. This bypasses client isolation.
2. **Gratuitous ARP Format:** Setting `Sender IP = Target IP = Gateway IP` and `Sender MAC = ESP32 MAC` creates a standard Gratuitous ARP packet. This format is universally accepted by operating systems for cache updates.
3. **`en_sys_seq = false`:** Prevents the Wi-Fi driver from overwriting our custom sequence numbers, ensuring the frame is valid.

---

## 5. The Working Attack Chain

### Step 1: Configuration
The user configures the ESP32 via Serial Monitor:
```
set_ssid teste
set_pass 12345678
set_target_ip 10.55.32.243
set_target_mac e0:d4:64:c2:c5:54
set_gateway_ip 10.55.32.206
set_gateway_mac b6:10:f4:de:b9:d4
save
connect
```

### Step 2: Start the Attack
```
start
```

### Step 3: What Happens on the Network
Every 500ms, the ESP32 injects two broadcast Gratuitous ARP Replies:
1. **"Gateway IP (10.55.32.206) is at ESP32 MAC"** → Laptop updates its cache
2. **"Target IP (10.55.32.243) is at ESP32 MAC"** → Gateway updates its cache

### Step 4: Verification on the Laptop
```bash
ip neigh show
```
**Result:**
```
10.55.32.206 dev wlp3s0 lladdr a4:f0:0f:5c:66:c0 REACHABLE
```
The gateway IP now maps to the **ESP32 MAC address**.

### Step 5: Traffic Interception
With the laptop ping running (`ping 8.8.8.8`), the ESP32 Serial Monitor shows:
```
[Forward] 10.55.32.243 -> 10.55.32.206 (84 bytes)
[Forward] 10.55.32.206 -> 10.55.32.243 (84 bytes)
```

The ESP32 receives ICMP request packets from the laptop, forwards them to the real gateway, receives the replies, and forwards them back to the laptop. The MitM loop is complete.

---

## 6. Why This Works Now (But Didn't Before)

| Factor | Initial State | Final State |
|--------|--------------|-------------|
| **Driver Filter** | `ieee80211_raw_frame_sanity_check` active | **Bypass override added** |
| **ARP Opcode** | 1 (Request) | **2 (Reply)** |
| **802.11 Delivery** | Unicast | **Broadcast** |
| **ARP Format** | Standard | **Gratuitous** (`Sender IP = Target IP`) |
| **AP Security** | Phone hotspot with client isolation | **Tested on open APs / router** |
| **Rate** | 1 packet every 2s | **Flood every 500ms** |

**The combination of ALL these fixes was required.** Any single missing piece would cause failure:
- Bypass without correct ARP format → frames reach air but OS ignores them
- Correct ARP format without bypass → driver drops frames before transmission
- Correct format + bypass but unicast → AP blocks with client isolation

---

## 7. Additional Capabilities Unlocked

With raw injection working, the ESP32 can now perform:

### A. Deauthentication (DoS)
- `deauth` command floods deauth frames
- Disconnects any client from the AP
- Uses 802.11 management frames (broadcast by AP)

### B. Wi-Fi Scanner
- `scan` command lists all nearby networks
- Uses built-in `WiFi.scanNetworks()` (always worked)

### C. Evil Twin (Rogue AP)
- Backup demonstration if ARP spoofing is blocked by a specific AP
- ESP32 creates its own network; traffic flows through it by design

---

## 8. Technical Limitations & Ethics

### Limitations
- **Throughput:** ESP32 RAM limits packet buffering. Rate limited to 10 forwarded packets/second to prevent crashes.
- **Unicast Visibility:** In STA mode, the ESP32 primarily captures broadcast/multicast and any unicast frames the AP happens to forward to it. Full unicast interception depends on the AP's behavior after ARP cache poisoning.
- **Encrypted Networks:** The attack works on WPA2-Personal where the ESP32 knows the PSK. It cannot break encryption.

### Ethics & Legal Notice
This tool is designed solely for:
- Cybersecurity education and training
- Authorized penetration testing in isolated lab environments
- Research into Layer 2 network security countermeasures

**Do not use on production networks, corporate environments, public WiFi, or any system you do not own.**

---

## 9. Conclusion

What began as a seemingly impossible challenge — ARP spoofing on an ESP32 — became a deep technical investigation into Wi-Fi driver internals, 802.11 frame formats, and Access Point security policies.

The key lessons:
1. **Embedded drivers have hidden restrictions.** Always check for known workarounds in the community.
2. **Protocol details matter.** A single wrong field (opcode 1 vs 2) can invalidate an entire attack.
3. **Network infrastructure fights back.** Modern APs have explicit anti-spoofing features.
4. **Persistence pays off.** After 5+ failed attempts across different frameworks, APs, and hardware, the combination of the right bypass + correct format + right environment finally worked.

**The ESP32 CAN perform ARP spoofing. It required bypassing the driver filter, using gratuitous ARP replies, and broadcasting the 802.11 frame to bypass AP client isolation.**

---

*Document generated by OpenCode as part of the ARP Spoofing lab project. Updated 2026-05-22 after successful ARP cache poisoning demonstration.*

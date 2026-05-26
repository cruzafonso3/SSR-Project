# ARP Spoofing / Man-in-the-Middle — Complete Documentation

> **Project:** Educational Man-in-the-Middle (MitM) Demonstration via ARP Spoofing  
> **Hardware:** ESP32 DOIT DevKit V1  
> **Framework:** PlatformIO (Arduino)  
> **Last Updated:** 2026-05-25

---

## 1. What Is This?

This is an **educational ARP Spoofing + DNS Spoofing tool** that runs on an ESP32 microcontroller. It demonstrates how a device on a Wi-Fi network can intercept traffic between a target laptop and the network gateway (router/access point) by poisoning their ARP caches. Once the MitM position is established, the ESP32 can:

- **Forward traffic** between the target and gateway transparently (the user sees no interruption)
- **Spoof DNS responses** to redirect website requests to a fake IP address
- **Log and inspect** packets in transit for educational analysis

All of this happens in **real time** on a tiny $5 microcontroller with 520KB of RAM and a single-core 240MHz Xtensa processor.

### Educational Purpose

This is designed for university cybersecurity lab courses to teach:
- How the **ARP protocol** works and why it has no authentication
- How **802.11 Wi-Fi frames** are structured at the byte level
- How **DNS** queries and responses work
- Why **encrypted DNS (DoH/DoT)** and **static ARP entries** are important defenses
- How embedded systems handle real-time packet processing

---

## 2. Architecture Overview

### Module Layout

```
main.cpp                    Entry point, FreeRTOS task creation, driver bypass
├── config.{h,cpp}          NVS-backed persistent settings
├── serial_cli.{h,cpp}      Interactive serial command-line interface
├── wifi_manager.{h,cpp}    Wi-Fi STA connection, MAC/BSSID retrieval
├── arp_engine.{h,cpp}      Periodic gratuitous ARP reply injection
├── arp_poisoner.{hpp,cpp}  Low-level ARP frame construction and transmission
├── packet_forwarder.{h,cpp} Promiscuous sniffing, packet capture, forwarding
└── dns_spoof.{h,cpp}       DNS query parsing and forged response generation
```

### Execution Model

- **`setup()`** — Initializes all modules and creates a single FreeRTOS task (`app_task`) pinned to Core 0
- **`app_task`** — Runs forever, calling `serial_cli_task()`, `arp_engine_task()`, and yielding every 10ms
- **`loop()`** — Just delays 100ms (all work happens in the FreeRTOS task)
- **Promiscuous callback** — Runs in Wi-Fi stack interrupt context; must be lightweight

### State Machine

```
IDLE ──connect──> CONNECTED ──start──> SPOOFING
  ^                  │                    │
  └──disconnect──────┘────stop───────────┘
```

- **IDLE**: No configuration, no Wi-Fi
- **CONNECTED**: Wi-Fi connected, configured, ready to start
- **SPOOFING**: ARP injection + packet forwarding + optional DNS spoofing active

---

## 3. How It Works (Step by Step)

### 3.1 Configuration Phase

The user connects to the ESP32 via serial monitor and sets up the environment:

```
set_ssid "MyHotspot"
set_pass "password123"
set_target_ip 192.168.43.243
set_target_mac e0:d4:64:c2:c5:54
set_gateway_ip 192.168.43.196
set_gateway_mac d2:6d:3b:f7:c8:04
save
connect
```

The ESP32 connects to the Wi-Fi hotspot as a normal Station (STA). It receives its own IP via DHCP.

### 3.2 ARP Cache Poisoning

When the user runs `start`, two things happen simultaneously:

**1. Periodically (every 500-1000ms), the ESP32 injects broadcast gratuitous ARP replies:**

```
Ethernet Header:
  Destination MAC: FF:FF:FF:FF:FF:FF  (broadcast)
  Source MAC:      A4:F0:0F:5C:66:C0  (ESP32's MAC)
  EtherType:       0x0806              (ARP)

ARP Header:
  Hardware Type:   1                   (Ethernet)
  Protocol Type:   0x0800              (IPv4)
  Opcode:          2                   (REPLY — critical!)
  Sender MAC:      A4:F0:0F:5C:66:C0  (ESP32 MAC — claiming to be gateway)
  Sender IP:       192.168.43.196      (gateway IP)
  Target MAC:      00:00:00:00:00:00   (gratuitous format)
  Target IP:       192.168.43.196      (gateway IP again)
```

**Why this works:**
- **Broadcast delivery** (`FF:FF:FF:FF:FF:FF`) causes the Access Point to flood the frame to ALL connected stations, bypassing client isolation
- **ARP Reply (opcode 2)** forces operating systems to update their ARP cache — ARP Requests are ignored for unsolicited updates
- **Gratuitous format** (Sender IP = Target IP) is a well-known ARP pattern that all OSes accept for cache updates
- **ESP32's MAC** is mapped to the **gateway's IP**, so the laptop sends all "outbound" traffic to the ESP32

**2. A second poison targets the gateway's cache:**

A second packet claims "target IP is at ESP32 MAC", so the gateway sends all return traffic to the ESP32.

### 3.3 Packet Forwarding (MitM Loop)

With the ESP32's MAC mapped to both IPs, all traffic between laptop and gateway flows through the ESP32:

```
Laptop ──ping 8.8.8.8──> ESP32 ──forwards──> Gateway ──internet──> 8.8.8.8
Laptop <──ESP32 forwards── ESP32 <──Gateway replies <──8.8.8.8
```

The ESP32 runs in **promiscuous mode** to capture all 802.11 data frames on its channel. The `packet_forwarder` module:

1. Captures raw 802.11 frames via `esp_wifi_set_promiscuous_rx_cb()`
2. Decodes the 802.11 header to extract source/destination MACs
3. Extracts the IP packet from the LLC/SNAP encapsulation
4. Checks if it's traffic between target and gateway (by IP)
5. Re-encapsulates the IP packet in a new 802.11 frame addressed to the real destination
6. Sends it via `esp_wifi_80211_tx()`

### 3.4 DNS Spoofing

When DNS spoofing is enabled, the forwarder intercepts UDP packets to port 53:

1. Parses the DNS query to extract the requested domain name
2. Checks if it matches the configured domain (`*` = all domains)
3. Constructs a complete DNS response with a forged A record pointing to the attacker's IP
4. Builds a valid IP + UDP + DNS packet from scratch (with correct checksums)
5. Injects it as a raw 802.11 frame back to the target

The DNS response has:
- Same transaction ID as the query (so it matches)
- Flags set to `0x8180` (response + authoritative + no error)
- One answer record with TTL of 60 seconds
- The forged IP address

Because the ESP32 is physically closer to the target than the real DNS server, the fake response usually arrives first.

---

## 4. Problems Encountered During Implementation

This project involved an extraordinarily difficult debugging process. Here is every major problem and how it was solved:

### Problem 1: The Hidden Driver Filter (Hardest Problem)

**Symptom:** `esp_wifi_80211_tx()` returned `ESP_OK` but no frames ever appeared on the air. Wireshark showed nothing.

**Root cause:** The ESP32's closed-source Wi-Fi driver (`libnet80211.a`) contains a function called `ieee80211_raw_frame_sanity_check()` that **silently discards** all raw 802.11 frames sent via `esp_wifi_80211_tx()` when the ESP32 is in Station mode. The function acts as a security filter, preventing raw frame injection.

**The deceptive part:** The function returns `ESP_OK` even when it drops the frame internally, making it look like everything works fine.

**Solution:** Override the function to always return 0 (success):

```cpp
extern "C" {
    int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
        return 0;
    }
}
```

The linker flag `-Wl,--allow-multiple-definition` is required because the function already exists in the closed-source library — the linker sees a duplicate symbol and must be told to accept the override.

**Verification:** After the bypass, a deauthentication (`deauth`) frame immediately disconnected the laptop from the network — proof that raw injection was working.

### Problem 2: Wrong ARP Opcode (Protocol Error)

**Symptom:** The laptop's ARP cache never updated.

**Root cause:** The code was sending ARP **Requests** (opcode 1). Operating systems ignore unsolicited ARP Requests for cache updates. Only ARP **Replies** (opcode 2) force a cache update.

**Solution:** Changed the opcode from 1 to 2.

### Problem 3: AP Client Isolation (Network-Level Block)

**Symptom:** Unicast frames addressed to the laptop were being dropped.

**Root cause:** Smartphone hotspots (Android/iOS) and modern routers implement **Client Isolation** — they drop STA-to-STA unicast data frames to prevent lateral movement on the network.

**Solution:** Send ARP replies as **broadcast frames** (destination `FF:FF:FF:FF:FF:FF`). The AP floods broadcast frames to all connected stations, bypassing client isolation entirely.

### Problem 4: AP ARP Inspection / IP Spoofing Detection

**Symptom:** Even with broadcast delivery, some APs dropped the poisoned ARP.

**Root cause:** Modern APs detect that a station is claiming the gateway's IP address and reject the contradictory claim. The AP knows the real gateway MAC and filters spoofed ARP packets.

**Solution:** Tested on open APs and router-based networks instead of phone hotspots. Also, the combination of **broadcast 802.11 + gratuitous ARP format + high frequency (every 500ms)** overwhelms the AP's filtering.

### Problem 5: ESP-IDF Rewrite Didn't Help

**Symptom:** Rewriting the entire project in ESP-IDF (pure C, no Arduino) produced the same failure.

**Root cause:** The restriction is not in the Arduino wrapper — it's in the **closed-source Wi-Fi binary blob** (`libnet80211.a`). Both Arduino and ESP-IDF ultimately call the same `esp_wifi_80211_tx()` function which invokes the same sanity check.

**Lesson:** Framework choice doesn't matter when the limitation is in the hardware abstraction layer.

### Problem 6: `en_sys_seq` Parameter

**Symptom:** Sporadic frame drops even with the bypass active.

**Root cause:** The `esp_wifi_80211_tx()` function's last parameter controls whether the driver overwrites the sequence number in the 802.11 header. When set to `true`, the driver could corrupt our custom frame.

**Solution:** Set `en_sys_seq = false` in the `esp_wifi_80211_tx()` call.

### Problem 7: Packet Forwarding Crashes

**Symptom:** After forwarding a few packets, the ESP32 would crash (watchdog timeout or panic).

**Root cause:** The promiscuous callback runs in **Wi-Fi interrupt context** — it must return quickly. The original code tried to do too much processing (including `Serial.print`) inside the callback, which violated the ISR timing constraints.

**Solution:** The callback now does minimal work: checks frame validity, copies the Ethernet frame, and queues it. All heavy processing (stats printing, DNS parsing) happens in the main task context.

### Problem 8: DNS Response Checksums

**Symptom:** DNS spoofed responses were received by the target but rejected as invalid.

**Root cause:** The IP and UDP checksums were calculated incorrectly. The DNS response had to be a fully valid IP/UDP/DNS packet with proper checksums.

**Solution:** Implemented correct RFC 1071 checksum calculation for both IP header checksum and UDP pseudo-header checksum.

---

## 5. ESP32 Hardware Limitations

### 5.1 Memory Constraints

| Resource | ESP32 | Usage |
|----------|-------|-------|
| **SRAM** | 520 KB | ~200 KB used by Wi-Fi stack, ~100 KB for sketch, remaining ~220 KB for packet buffers |
| **Flash** | 4 MB | More than enough for firmware + SPIFFS |
| **Heap** | ~200 KB free | Fragmentation is a concern with dynamic allocation |

**Impact:** We cannot buffer more than a few dozen packets. Rate limiting is essential.

### 5.2 Single Radio

The ESP32 has only one Wi-Fi radio. While in promiscuous mode:
- It cannot simultaneously maintain the Wi-Fi connection
- It is locked to one channel at a time
- It misses packets on other channels

**Impact:** The ESP32 must periodically switch between promiscuous reception and normal STA operation. We stay on the AP's channel so we don't miss association.

### 5.3 CPU Performance

- **CPU:** Xtensa LX6 dual-core @ 240 MHz
- **For this project:** Core 0 handles the application task; Core 1 handles Wi-Fi stack
- **Interrupt context:** The promiscuous callback runs in high-priority interrupt context — minimal processing allowed

**Impact:** Maximum sustainable forwarding rate is approximately **10 packets/second**. Attempting higher rates triggers watchdog timeouts or packet drops.

### 5.4 Closed-Source Driver

The Wi-Fi driver is distributed as a precompiled binary blob (`libnet80211.a`, `libpp.a`). Key limitations:
- No source code access for debugging
- The `ieee80211_raw_frame_sanity_check` bypass is a known community workaround but is not officially supported
- Future ESP-IDF/Arduino updates may change the function signature or behavior

**Impact:** The driver bypass is inherently fragile. It works on ESP-IDF 4.x / 5.x but may break in future releases.

### 5.5 No Hardware Crypto Acceleration for WPA Cracking

The ESP32 has hardware AES/SHA acceleration, but it is not exposed in a way that would be useful for WPA handshake cracking. The device can capture handshakes but cannot crack them — that requires x86 GPU/CPU power and must be done on a laptop.

### 5.6 USB-UART Serial Bottleneck

The serial monitor runs at 115200 baud (~11.5 KB/s). This is fine for CLI interaction but too slow for real-time packet dumping.

---

## 6. How to Test

### 6.1 Prerequisites

**Hardware:**
- ESP32 DOIT DevKit V1
- A laptop/computer to serve as the target
- A Wi-Fi access point (router or hotspot) — preferably not a phone hotspot (client isolation issues)
- USB cable for serial communication

**Software:**
- PlatformIO (or Arduino IDE with ESP32 support)
- Serial monitor (PlatformIO's built-in, PuTTY, screen, etc.)
- Wireshark (on the laptop, for verification)
- Python 3 (for packet analysis tools)

### 6.2 Build and Flash

```bash
cd "ARP SPOOF"
pio run
pio run --target upload
pio device monitor
```

### 6.3 Test Plan

#### Test 1: Raw Frame Injection (Verify Driver Bypass)

This is the most fundamental test — if it fails, nothing else works.

1. Flash the ESP32 and open the serial monitor
2. Connect to Wi-Fi:
   ```
   set_ssid YourHotspot
   set_pass YourPassword
   connect
   ```
3. Run a "test injection" (send a raw 802.11 probe request):
   ```
   test_raw
   ```
4. On the laptop, run Wireshark on the Wi-Fi interface
5. Filter: `wlan.fc.type_subtype == 4` (Probe Request)
6. Look for frames from the ESP32's MAC address

**Expected:** The probe request appears in Wireshark.  
**Failure:** If nothing appears, the driver bypass is not working (check build flags).

#### Test 2: ARP Cache Poisoning

1. Ensure the laptop is connected to the same Wi-Fi
2. Find the laptop's IP and MAC:
   ```bash
   ip addr show       # Find your IP
   ip link show       # Find your MAC
   ```
3. Find the gateway IP and BSSID:
   ```bash
   ip route show default    # Gateway IP
   iw dev wlan0 link        # BSSID (AP MAC)
   ```
4. Configure the ESP32:
   ```
   set_target_ip 192.168.43.243
   set_target_mac e0:d4:64:c2:c5:54
   set_gateway_ip 192.168.43.196
   set_gateway_mac d2:6d:3b:f7:c8:04
   save
   ```
5. Start the attack:
   ```
   start
   ```
6. On the laptop, check the ARP cache:
   ```bash
   ip neigh show
   ```

**Expected:** The gateway's IP now maps to the **ESP32's MAC address** instead of the real gateway MAC.  
**Failure:** If the ARP cache still shows the real gateway MAC, check:
   - The driver bypass is working (Test 1)
   - The ARP opcode is set to 2 (Reply)
   - The ESP32 is sending broadcast frames (not unicast)
   - The AP is not filtering gratuitous ARP

**Expected output:**
```
192.168.43.196 dev wlp3s0 lladdr a4:f0:0f:5c:66:c0 REACHABLE
```

#### Test 3: Traffic Interception (MitM)

1. Start the attack (Test 2)
2. On the laptop, run a continuous ping:
   ```bash
   ping 8.8.8.8
   ```
3. On the ESP32 serial monitor, check:
   ```
   status
   ```

**Expected:** The packet counter shows captured and forwarded packets increasing:
```
Stats: Cap=42 Fwd=40 Drop=0 DNS=0
```

**Failure:** Packets are captured but not forwarded:
   - Check that both target and gateway MACs are correct
   - Check that the AP is forwarding broadcast packets to the laptop
   - Try increasing ARP injection frequency

#### Test 4: DNS Spoofing

1. Set up DNS spoofing:
   ```
   set_dns_ip 192.168.43.1
   set_dns_domain *
   dns_start
   ```
2. On the laptop, run:
   ```bash
   nslookup google.com
   ```
   or
   ```bash
   dig google.com
   ```

**Expected:** The response shows `192.168.43.1` as the IP for `google.com`:
```
Server:     192.168.43.1
Address:    192.168.43.1#53

Non-authoritative answer:
Name:       google.com
Address:    192.168.43.1
```

**Failure:** If the real IP is returned:
   - The DNS response might be arriving after the real server's response
   - Check that `dns_start` was run after `start`
   - Check that the DNS query is UDP port 53 (not DNS-over-HTTPS)
   - Try `nslookup` multiple times rapidly

#### Test 5: Persistence and Recovery

1. While spoofing is active, disconnect the ESP32 from Wi-Fi:
   ```
   disconnect
   ```
2. Check that all engines stop:
   ```
   status
   ```
3. Reconnect and restart:
   ```
   connect
   start
   ```

**Expected:** The ESP32 gracefully stops all engines on disconnect and can be restarted without crashing.  
**Failure:** Crash or hang on disconnect indicates a null-pointer dereference or race condition.

### 6.4 Verification with Wireshark

On the laptop, run Wireshark and apply these filters:

**To see ARP poison packets:**
```
arp
```
You should see periodic gratuitous ARP replies from the ESP32's MAC claiming the gateway's IP.

**To see forwarded traffic:**
```
ip.addr == 8.8.8.8
```
You should see ping requests from the laptop and replies from the internet — but the MAC addresses will alternate between the ESP32 and the real gateway.

**To see DNS spoofing:**
```
dns.qry.name == google.com
```
You should see the ESP32's DNS response arriving before the real DNS server's response.

### 6.5 Cleaning Up

After testing, restore the laptop's ARP cache:
```bash
sudo ip neigh flush dev wlan0
```

Or reboot. The ARP changes are temporary and will revert within minutes once the ESP32 stops sending poisoned packets.

---

## 7. Limitations of This Implementation

| Limitation | Reason | Impact |
|------------|--------|--------|
| **10 pkt/sec forwarding max** | ESP32 CPU + interrupt constraints | Cannot handle high-throughput connections (video streaming, large downloads) |
| **Single channel** | ESP32 has one radio | Cannot follow channel-hopping networks |
| **Fragile driver bypass** | Closed-source driver binary | May break on ESP-IDF updates |
| **AP dependent** | Requires non-isolating AP | Phone hotspots generally don't work |
| **No HTTPS decryption** | Traffic is encrypted | Can only inspect SNI/domain, not content |
| **Manual MAC/IP setup** | No automated discovery | Must configure target/gateway manually |
| **No encryption breaking** | WPA2-PSK only protects link layer | Cannot decrypt target's traffic |

---

## 8. Defensive Countermeasures

What this lab teaches students to defend against:

1. **Static ARP entries** — Set `arp -s <gateway_ip> <gateway_mac>` to prevent cache poisoning
2. **Dynamic ARP Inspection (DAI)** — Enterprise switches can validate ARP packets
3. **Encrypted DNS (DoH/DoT)** — DNS-over-HTTPS or DNS-over-TLS prevents spoofing
4. **WPA3** — Includes SAE (Simultaneous Authentication of Equals) with built-in MitM protection
5. **VPN** — Traffic encrypted end-to-end, even if the link layer is compromised
6. **AP Client Isolation** — Enabled on guest networks prevents this attack entirely

---

## 9. Files Reference

| File | Purpose |
|------|---------|
| `platformio.ini` | Build configuration with driver bypass linker flag |
| `src/main.cpp` | Entry point, FreeRTOS task, driver bypass |
| `src/config.cpp` | NVS configuration save/load/reset |
| `src/config.h` | AppConfig struct and extern declarations |
| `src/serial_cli.cpp` | Interactive CLI with 15+ commands |
| `src/serial_cli.h` | CLI function declarations |
| `src/wifi_manager.cpp` | STA mode connection, MAC/BSSID retrieval |
| `src/wifi_manager.h` | WiFi management function declarations |
| `src/arp_engine.cpp` | Periodic ARP injection orchestrator |
| `src/arp_engine.h` | ARP engine function declarations |
| `src/arp_poisoner.cpp` | Low-level ARP frame construction |
| `src/arp_poisoner.hpp` | ARP frame structures and class definition |
| `src/packet_forwarder.cpp` | Promiscuous capture, forwarding, DNS interception |
| `src/packet_forwarder.h` | Forwarder function declarations |
| `src/dns_spoof.cpp` | DNS query parser + forged response generator |
| `src/dns_spoof.h` | DNS spoof function declarations |
| `find_network_info.sh` | Helper script to auto-detect target/gateway info |

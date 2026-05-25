# DHCP Starvation Attack — Complete Documentation

> **Project:** Educational DHCP Starvation Attack Demonstration  
> **Hardware:** ESP32 DOIT DevKit V1  
> **Framework:** PlatformIO (Arduino)  
> **Last Updated:** 2026-05-25

---

## 1. What Is This?

This is an **educational DHCP (Dynamic Host Configuration Protocol) starvation tool** that runs on an ESP32. It demonstrates a Layer 2 denial-of-service attack where the attacker exhausts all available IP addresses in a DHCP server's pool, preventing legitimate clients from obtaining network connectivity.

The attack works by **spoofing MAC addresses** — the ESP32 rapidly changes its MAC address and requests a new DHCP lease for each one, consuming every available IP in the pool.

### How DHCP Normally Works

When a device connects to a Wi-Fi network:
1. **DHCP Discover** — Client broadcasts: "I need an IP address"
2. **DHCP Offer** — Server responds: "Here, use 192.168.43.X"
3. **DHCP Request** — Client: "I accept 192.168.43.X"
4. **DHCP Ack** — Server: "Confirmed, lease for 24 hours"

The server maps each **MAC address** to an **IP address** and records the lease duration. Each unique MAC can only get one IP.

### The Attack

The ESP32:
1. Connects to the Wi-Fi network with its real MAC and gets an IP
2. Changes its MAC address to a random locally-administered MAC
3. Reconnects to the network — the DHCP server sees a "new" client and assigns a new IP
4. Repeats this every few hundred milliseconds with a different random MAC each time
5. Eventually the DHCP pool is exhausted — no more IPs available for legitimate clients

### Educational Purpose

- Understand how DHCP works at the protocol level
- Learn why MAC address spoofing is a security concern
- See the impact of DHCP pool exhaustion on network availability
- Understand countermeasures like DHCP snooping and port security

---

## 2. Architecture Overview

### Module Layout

```
main.cpp                    Entry point, FreeRTOS task, DHCPGlutton management
├── wifi_manager.{h,cpp}    Wi-Fi STA connection, MAC retrieval
├── serial_cli.{h,cpp}      Interactive serial CLI, Preferences storage
└── dhcp_glutton.{h,cpp}    Core DHCP starvation engine
```

### Execution Model

- **`setup()`** — Initializes WiFi, serial CLI, creates `app_task` on Core 0
- **`app_task`** — Every 100ms, runs CLI handler and calls `glutton->step()` if running
- **`loop()`** — Simply delays 1000ms (all work in FreeRTOS task)
- **`DHCPGlutton`** — Stateful class that manages the starvation cycle

### Class: `DHCPGlutton`

```
DHCPGlutton
├── ssid / password        -- Target network credentials
├── cooldown_ms            -- Delay between reconnect attempts
├── original_mac[6]        -- Saved real MAC (restored on stop)
├── random_mac[6]          -- Current random MAC
├── reconnect_count        -- Total successful DHCP leases obtained
├── running                -- Boolean state flag
└── Methods:
    ├── start()            -- Begin starvation
    ├── stop()             -- Stop, restore MAC
    ├── step()             -- One starvation cycle
    └── generate_random_mac() -- Create locally-administered MAC
```

---

## 3. How It Works (Step by Step)

### 3.1 Configuration Phase

```
set_ssid MyHotspot
set_pass password123
set_cooldown 1000
save
```

The cooldown controls the delay between reconnect attempts:
- **500ms minimum** — Very aggressive, may overwhelm the AP
- **1000ms default** — Steady, 1 IP per second = 253 IPs in ~4 minutes
- **10000ms maximum** — Slow, 1 IP per 10 seconds

### 3.2 The Starvation Cycle

When `start` is executed, the `DHCPGlutton.step()` method is called every 100ms (from the main loop).

Each cycle:

1. **Check cooldown**: If not enough time has passed since the last reconnect, skip this cycle

2. **Generate random MAC**:
   ```cpp
   void DHCPGlutton::generate_random_mac() {
       randomSeed(millis());
       for (int i = 0; i < 6; i++) {
           random_mac[i] = random(0, 256);
       }
       random_mac[0] |= 0x02;   // Set locally administered bit
       random_mac[0] &= 0xFE;   // Clear multicast bit
   }
   ```
   The **locally administered bit** (second-least significant bit of the first byte) marks this as a software-generated MAC, not a vendor-assigned one. This is important because legitimate devices have OUI-assigned MACs — the AP could theoretically filter on this, though most don't.

3. **Change MAC address**:
   ```cpp
   esp_wifi_set_mac(WIFI_IF_STA, random_mac);
   ```
   This tells the Wi-Fi driver to use the new MAC for the next connection. The ESP32's Wi-Fi driver allows runtime MAC changes; many network adapters do not.

4. **Disconnect from current session**:
   ```cpp
   WiFi.disconnect(false);  // false = don't erase stored AP config
   ```

5. **Reconnect with new MAC**:
   ```cpp
   WiFi.begin(ssid, password);
   ```
   The AP sees a completely new device (new MAC) and initiates the 4-way DHCP handshake automatically.

6. **Wait for connection** (up to 5 seconds, with 50 retries at 100ms each):
   - If connected: Increment counter, log the IP and MAC
   - If timeout: Log failure and retry on next cycle

7. **Repeat**: Cycle after cycle, consuming one IP per cycle

### 3.3 IP Address Exhaustion

A typical /24 subnet (netmask 255.255.255.0) has **254 usable addresses** (1 for network, 1 for broadcast, and the gateway uses one). Standard DHCP server configurations allocate from a pool of 100-253 addresses.

| Cooldown | Time to exhaust 100 IPs | Time to exhaust 253 IPs |
|----------|------------------------|--------------------------|
| 500ms    | ~50 seconds            | ~2 minutes               |
| 1000ms   | ~1.7 minutes           | ~4.2 minutes             |
| 5000ms   | ~8.3 minutes           | ~21 minutes              |

### 3.4 Cleanup on Stop

When `stop` is called:
1. The starvation flag is cleared
2. The ESP32's original MAC is restored:
   ```cpp
   esp_wifi_set_mac(WIFI_IF_STA, original_mac);
   ```
3. The `DHCPGlutton` object is destroyed
4. The ESP32 returns to normal operation with its real MAC

Without MAC restoration, the ESP32 would be "stuck" with a random MAC until the next reboot.

---

## 4. Problems Encountered During Implementation

### Problem 1: MAC Change Fails

**Symptom:** `esp_wifi_set_mac()` returned `ESP_ERR_INVALID_ARG` or `ESP_ERR_INVALID_VERSION`.

**Root cause:** The ESP32's Wi-Fi driver must be in a specific state to change the MAC:
- The MAC can only be changed when the interface is **down** (not connected)
- The MAC can only be changed before the Wi-Fi stack is fully initialized

**Solution:** Call `esp_wifi_set_mac()` after `WiFi.disconnect()` but before `WiFi.begin()`. The disconnect puts the interface in the right state for a MAC change.

### Problem 2: Reconnection Timeout

**Symptom:** The ESP32 fails to reconnect after MAC changes, especially at high speeds.

**Root cause:** The 4-way DHCP handshake has timing constraints. If the AP is busy (processing many DHCP requests), it may not respond in time. Additionally, the ESP32's internal state machine sometimes takes longer than expected to transition from "disconnected" to "connecting".

**Solution:** 
- Increased the retry window from 20 retries (2s) to 50 retries (5s)
- Added a 100ms delay between disconnect and reconnect to let the ESP32 settle
- The cooldown minimum is 500ms to prevent overwhelming the ESP32 itself

### Problem 3: Random MAC Collisions

**Symptom:** Occasionally the ESP32 would generate a MAC that was already in use (either by a real device or by a previous spoofed attempt).

**Root cause:** The random MAC generation is entirely random — there's no collision detection. With 2^46 possible locally-administered MACs, collisions are rare on a small network but possible.

**Solution:** Accepted as a minor issue. In practice, collisions are extremely unlikely on a network with fewer than 100 devices. A collision would simply mean the DHCP server doesn't assign a new IP (it sees the existing lease).

### Problem 4: AP Rate Limiting

**Symptom:** After 50-100 successful reconnects, the rate of new IP assignments drops dramatically.

**Root cause:** Many APs implement rate limiting on DHCP — they detect rapid reconnect attempts from the same physical device (same radio signature) and throttle or block further requests.

**Solution:** Increased the cooldown to give the AP time to process requests. For particularly aggressive APs, a random jitter (cooldown +/- 200ms) could help avoid detection patterns.

### Problem 5: Memory Leak (Fixed)

**Symptom:** The ESP32 would crash after thousands of reconnections due to heap exhaustion.

**Root cause:** Each `WiFi.begin()` call allocates some internal buffers. While `disconnect` frees most of them, there was a small leak that accumulated over many cycles.

**Solution:** After every 100 reconnections, the code forces a full Wi-Fi stack restart:
```cpp
if (reconnect_count % 100 == 0) {
    esp_wifi_stop();
    delay(10);
    esp_wifi_start();
}
```

---

## 5. ESP32 Hardware Limitations

### 5.1 MAC Address Restrictions

The ESP32 uses the MAC stored in its eFuse (one-time programmable memory). The `esp_wifi_set_mac()` function changes the runtime MAC but:
- The eFuse MAC is still the "real" MAC
- Some APs can detect the original MAC through the Wi-Fi association process
- The ESP32 has 3 MAC addresses (Wi-Fi STA, Wi-Fi AP, Bluetooth) — changing one does not affect the others

### 5.2 Wi-Fi Stack Overhead

Each reconnection cycle involves:
- Disconnecting from the AP
- Changing the MAC
- Scanning for the AP
- Authenticating (WPA2 4-way handshake)
- DHCP Discover/Offer/Request/Ack

This whole process takes **1-5 seconds** depending on signal strength and AP responsiveness. The ESP32's Wi-Fi stack is relatively fast compared to consumer devices but is still limited by the AP's processing speed.

### 5.3 Connection State Tracking

The ESP32's Wi-Fi stack was not designed for rapid reconnection cycling. Internal state machines, timers, and buffer pools are optimized for normal usage (occasional reconnects, roaming). After many rapid cycles, the internal state can become inconsistent:
- The Wi-Fi event handler may miss connection/disconnection events
- The internal DHCP client may hang waiting for a response that was already processed
- The 802.11 association timeout may fire at unexpected times

**Workaround:** The 100ms delay between disconnect and reconnect allows the internal state machine to settle.

### 5.4 No Simultaneous Connections

The ESP32 has one radio. While it is connected as a station to the target AP, it cannot:
- Maintain its own AP mode (be a hotspot)
- Communicate over Bluetooth
- Scan for other networks

This means the ESP32 is "offline" during the starvation attack — you can only monitor progress via serial.

### 5.5 Limited Logging

With serial at 115200 baud and a reconnect every 1 second, the serial output is approximately 100 characters per reconnect = ~10,000 characters/second = 80,000 baud. This is within the 115200 limit but leaves no room for verbose debugging.

### 5.6 SRAM Fragmentation

The `DHCPGlutton` object is dynamically allocated (`new` / `delete`) each time `start` / `stop` is called. While the object is small (~100 bytes), the associated Wi-Fi stack allocations cause heap fragmentation over time.

---

## 6. How to Test

### 6.1 Prerequisites

**Hardware:**
- ESP32 DOIT DevKit V1
- A Wi-Fi access point with a DHCP server (almost any consumer router)
- A device to observe the impact (laptop trying to connect)
- USB cable for serial

**Software:**
- PlatformIO
- Serial monitor
- (Optional) Wireshark for DHCP packet observation

### 6.2 Build and Flash

```bash
cd "DHCP Starvation"
pio run
pio run --target upload
pio device monitor
```

### 6.3 Test Plan

#### Test 1: Basic Single Connection

Verify the ESP32 can connect to the target network normally.

1. Flash the ESP32 with the starvation code
2. Configure and connect:
   ```
   set_ssid MyHotspot
   set_pass MyPassword
   connect
   ```
3. Verify connection:
   ```
   status
   ```

**Expected:**
```
WiFi: Connected
ESP32 MAC: A4:F0:0F:5C:66:C0
```

#### Test 2: Single MAC Spoof Attempt

Verify MAC change works.

1. Connect normally (Test 1)
2. Note the ESP32's IP
3. Start starvation for just a few seconds:
   ```
   start
   ```
4. Wait 3-4 seconds, then stop:
   ```
   stop
   ```

**Expected:**
```
[DHCP] #1 -> IP: 192.168.43.157  MAC: 02:3A:5B:8C:1D:4F
[DHCP] #2 -> IP: 192.168.43.158  MAC: 02:7C:9E:1A:3F:6B
[DHCP] Stopped. Original MAC restored.
```

Each line shows a successful DHCP lease with a random MAC.

#### Test 3: Continuous Starvation

Observe the IP pool depletion rate.

1. Set cooldown to 1000ms (1 second)
2. Start starvation:
   ```
   start
   ```
3. Let it run and observe the counter increment:
   ```
   [DHCP] #10 -> IP: 192.168.43.166  MAC: 02:4D:8F:2A:1C:7E
   [DHCP] #11 -> IP: 192.168.43.167  MAC: 02:9B:3E:7C:5F:0D
   ...
   ```

4. Check status periodically:
   ```
   status
   ```
   **Expected:**
   ```
   State: RUNNING
   Reconnects: 42
   Current IP: 192.168.43.198
   Current MAC: 02:F1:6A:3D:8B:4E
   ```

#### Test 4: Impact Verification (Exhaustion)

This test confirms the attack actually prevents legitimate clients from connecting.

1. Before starting the attack, disconnect a laptop from the Wi-Fi
2. Start the ESP32 starvation attack
3. After 2-3 minutes (assuming ~100 IP pool), try to connect the laptop to the Wi-Fi

**Expected:** The laptop fails to obtain an IP address, showing:
- "Unable to join network"
- "IP configuration failure"
- `dhclient` hangs indefinitely
- Or the laptop gets a **169.254.x.x** APIPA address (self-assigned, no internet access)

**Verification on the laptop:**
```bash
ip addr show
```
If the laptop has a 169.254.x.x address, DHCP has failed.

#### Test 5: Clean Restoration

After `stop`, verify the ESP32 returns to normal operation.

1. Stop the attack
2. Check status:
   ```
   status
   ```
3. Verify the MAC is restored by checking the router's DHCP client list

**Expected:**
```
State: Stopped
ESP32 MAC: A4:F0:0F:5C:66:C0
```
The ESP32's MAC matches its original hardware MAC (printed on the module).

#### Test 6: DHCP Recovery After Attack

1. Stop the ESP32 attack
2. Reboot the router's DHCP server (or wait for lease timeouts)
3. Try connecting the laptop again

**Expected:** The laptop receives an IP address normally. DHCP leases will expire (typically 1-24 hours), freeing IPs back to the pool.

### 6.4 Verification with Wireshark

1. Start Wireshark on the laptop's Wi-Fi interface
2. Filter: `bootp` (DHCP packets use the BOOTP protocol)
3. Observe the flood of DHCP Discover/Request packets from different MAC addresses

**You should see:**
```
DHCP Discover - Transaction ID: 0x3A1F... Client MAC: 02:4D:8F:2A:1C:7E
DHCP Offer    - Transaction ID: 0x3A1F... Your IP: 192.168.43.166
DHCP Request  - Transaction ID: 0x3A1F... Client MAC: 02:4D:8F:2A:1C:7E
DHCP Ack      - Transaction ID: 0x3A1F... Your IP: 192.168.43.166
DHCP Discover - Transaction ID: 0x7B9C... Client MAC: 02:9B:3E:7C:5F:0D
DHCP Offer    - Transaction ID: 0x7B9C... Your IP: 192.168.43.167
...
```

Each 4-message exchange consumes one IP from the pool.

### 6.5 Router/AP Observation

On the router's admin interface, check:
1. **DHCP Client List** — Should show many clients with `02:` starting MACs (locally administered)
2. **DHCP Pool Usage** — Should be near 100%
3. **System Log** — May show "DHCP pool exhausted" warnings

---

## 7. Limitations of This Implementation

| Limitation | Reason | Impact |
|------------|--------|--------|
| **Rate limited to 1 IP/s default** | ESP32 processing + AP overhead | Takes ~4 min to exhaust a /24 pool |
| **No collision detection** | Random MAC generation | Tiny chance of duplicate MAC |
| **AP rate limiting** | AP firmware defense | Attack slows after ~100 leases |
| **No multi-AP support** | One radio | Can only attack one network at a time |
| **No persistence** | RAM-only state | Reboot resets everything |
| **Serial monitor required** | No web interface | Must be physically connected via USB |

---

## 8. Defensive Countermeasures

What this lab teaches students to defend against:

1. **DHCP Snooping** — Enterprise switches can filter DHCP packets from untrusted ports
2. **Port Security** — Limit the number of MAC addresses allowed per switch port
3. **MAC Address Filtering** — Only allow known OUI-assigned MACs (block locally administered `02:x:x:x:x:x`)
4. **Rate Limiting** — Limit DHCP requests per second from a single physical port
5. **DHCP Pool Monitoring** — Alert when pool usage exceeds a threshold
6. **802.1X** — Port-based authentication ensures only authorized devices connect
7. **Larger DHCP Pool** — More addresses = longer time to exhaust (mitigation, not prevention)

---

## 9. Files Reference

| File | Purpose |
|------|---------|
| `platformio.ini` | Build configuration for ESP32 + serial |
| `src/main.cpp` | Entry point, FreeRTOS task, glutton lifecycle |
| `src/dhcp_glutton.cpp` | MAC spoofing + DHCP starvation core engine |
| `src/dhcp_glutton.h` | DHCPGlutton class definition |
| `src/serial_cli.cpp` | CLI with 9 commands + NVS config persistence |
| `src/serial_cli.h` | CLI function declarations |
| `src/wifi_manager.cpp` | Wi-Fi connection management |
| `src/wifi_manager.h` | WiFi function declarations |

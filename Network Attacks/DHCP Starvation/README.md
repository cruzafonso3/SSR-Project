# DHCP Starvation Attack — Complete Documentation

> **Project:** Educational DHCP Starvation Attack Demonstration  
> **Hardware:** ESP32 DOIT DevKit V1  
> **Framework:** PlatformIO (Arduino)  
> **Last Updated:** 2026-05-26

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
src/
├── main.cpp                Entry point, FreeRTOS task, module wiring
├── config.{h,cpp}          AppConfig struct + NVS persistence + version hash
├── dhcp_starvation.{h,cpp} DHCP starvation engine (module API)
├── wifi_manager.{h,cpp}    Wi-Fi STA connection, MAC retrieval
├── serial_cli.{h,cpp}      Interactive serial CLI (fallback)
├── display_manager.{h,cpp} SSD1306 OLED drawing helpers
├── buttons.{h,cpp}         GPIO button debounce/repeat/double-click
└── ui_menu.{h,cpp}         OLED menu system
```

### Execution Model

- **`setup()`** — Initializes config, WiFi, starvation engine, serial CLI, OLED, buttons, menu; creates `app_task` on Core 0
- **`app_task`** — Every 100ms: runs CLI handler, calls `dhcp_step()`, processes button input, redraws OLED
- **`dhcp_step()`** — Manages the starvation cycle internally with static state (no global accessor functions)

### Module API

```
dhcp_init()                    Save original MAC, reset counters
dhcp_start()                   Begin starvation cycle
dhcp_stop()                    Stop cycle, restore original MAC
dhcp_is_running()              Boolean check
dhcp_step()                    One starvation cycle (called from app_task)
dhcp_get_reconnect_count()     Total successful DHCP leases
dhcp_get_consecutive_failures() Failed attempts since last success
dhcp_is_pool_full()            True when ≥10 consecutive failures
dhcp_get_current_ip()          Last IP obtained
dhcp_get_current_mac()         Current spoofed MAC
dhcp_get_original_mac()        Real ESP32 hardware MAC
```

All state is module-static — no class, no accessor wrappers, no `get_glutton()` / `create_glutton()` / `destroy_glutton()`.

---

## 3. How It Works (Step by Step)

### 3.1 Configuration Phase

Set via OLED menu (Settings → SSID / Password / Cooldown) or serial CLI:
```
set_ssid MyHotspot
set_pass password123
set_cooldown 500
save
```

The cooldown controls the delay between reconnect attempts:
- **500ms default** — Steady, 2 IPs per second
- **10000ms maximum** — Slow, 1 IP per 10 seconds

### 3.2 The Starvation Cycle

Each cycle of `dhcp_step()`:

1. **Check cooldown**: If not enough time has passed since the last reconnect, skip this cycle

2. **Generate random MAC**:
   ```cpp
   randomSeed(millis());
   for (int i = 0; i < 6; i++) random_mac[i] = random(0, 256);
   random_mac[0] |= 0x02;   // Locally administered bit
   random_mac[0] &= 0xFE;   // Clear multicast bit
   ```
   The **locally administered bit** marks this as a software-generated MAC, not a vendor-assigned one.

3. **Change MAC address**:
   ```cpp
   esp_wifi_set_mac(WIFI_IF_STA, random_mac);
   ```

4. **Disconnect** (preserving AP config):
   ```cpp
   WiFi.disconnect(false);
   ```

5. **Reconnect with new MAC** — The AP sees a completely new device and initiates DHCP.

6. **Wait for connection** (up to 5 seconds, 50 retries at 100ms each):
   - If connected → `reconnect_count++`, `consecutive_failures = 0`, log IP/MAC
   - If timeout → `consecutive_failures++`, log failure

7. **Repeat** — Cycle after cycle, consuming one IP per cycle.

### 3.3 IP Address Exhaustion

A typical /24 subnet has **254 usable addresses**. Standard DHCP servers allocate from a pool of 100-253 addresses.

| Cooldown | Time to exhaust 100 IPs | Time to exhaust 253 IPs |
|----------|------------------------|--------------------------|
| 500ms    | ~50 seconds            | ~2 minutes               |
| 1000ms   | ~1.7 minutes           | ~4.2 minutes             |
| 5000ms   | ~8.3 minutes           | ~21 minutes              |

### 3.4 Pool Exhaustion Detection

When **10+ consecutive reconnects fail** to obtain a DHCP lease, `dhcp_is_pool_full()` returns `true`. The OLED shows `FULL: YES (10+ fails)` and the status page shows `POOL FULL?: YES`.

This indicates the DHCP pool is likely exhausted — the server has no more addresses to give.

### 3.5 Cleanup on Stop

When `stop` is called:
1. The starvation flag is cleared
2. The ESP32's original MAC is restored via `esp_wifi_set_mac()`
3. Counters are preserved for status display

Without MAC restoration, the ESP32 would be "stuck" with a random MAC until the next reboot.

---

## 4. Problems Encountered During Implementation

### Problem 1: MAC Change Fails

**Symptom:** `esp_wifi_set_mac()` returns `ESP_ERR_INVALID_ARG`.

**Root cause:** The MAC can only be changed when the interface is down (not connected).

**Solution:** Call after `WiFi.disconnect()` but before `WiFi.begin()`.

### Problem 2: Reconnection Timeout

**Symptom:** The ESP32 fails to reconnect after MAC changes, especially at high speeds.

**Solution:** 
- Increased retry window to 50 retries (5s)
- Added 100ms delay between disconnect and reconnect
- Cooldown minimum is 500ms

### Problem 3: Random MAC Collisions

**Symptom:** Occasionally the ESP32 generates a MAC already in use.

**Solution:** Accepted as minor — 2^46 possible locally-administered MACs; collisions are extremely unlikely on a network with fewer than 100 devices.

### Problem 4: AP Rate Limiting

**Symptom:** After 50-100 reconnects, the rate of new IP assignments drops.

**Solution:** Increase cooldown or add random jitter (cooldown +/- 200ms) to avoid detection.

### Problem 5: Memory Leak

**Symptom:** ESP32 crashes after thousands of reconnections due to heap exhaustion.

**Solution:** After every 100 reconnections, force a full Wi-Fi stack restart:
```cpp
if (reconnect_count % 100 == 0) {
    esp_wifi_stop();
    delay(10);
    esp_wifi_start();
}
```

---

## 5. OLED Interface

### Hardware

| Component | Pin |
|-----------|-----|
| OLED SDA | GPIO 21 |
| OLED SCL | GPIO 22 |
| Button UP | GPIO 14 |
| Button DOWN | GPIO 27 |
| Button SELECT | GPIO 26 |

### Menu

```
  Connect WiFi          ← connects using saved SSID/pass
  Disconnect WiFi       ← stops attack if running, disconnects
  Start Starvation      ← enters live attack screen
  Stop Starvation       ← stops and restores MAC
  Status                ← scrollable: all info including pool status
  Settings              ← SSID / Password / Cooldown / Factory Reset
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

- Pressing SELECT or BACK on this screen stops the attack.
- Pressing BACK from the main menu leaves the attack running.
- Exiting and returning to Status shows fresh live data.

### Auto-Reset on Reflash

A build version hash (`__DATE__ __TIME__` djb2 hash) is stored in NVS. Every reflash changes the hash, triggering an automatic factory reset of all config. No stale SSIDs, passwords, or settings survive a firmware update.

---

## 6. How to Test

### 6.1 Prerequisites

**Hardware:**
- ESP32 DOIT DevKit V1
- OLED SSD1306 128x64 + 3 buttons wired as above
- A Wi-Fi access point with a DHCP server
- USB cable for serial (optional, for debugging)

**Software:**
- PlatformIO

### 6.2 Build and Flash

```bash
cd "Network Attacks/DHCP Starvation"
pio run
pio run --target upload
```

### 6.3 Test Plan

#### Test 1: Basic Single Connection

1. Configure via Settings menu: SSID, Password
2. Select "Connect WiFi"
3. Verify "WiFi connected" message

#### Test 2: Single MAC Spoof Attempt

1. Connect normally (Test 1)
2. Select "Start Starvation"
3. Observe the live screen incrementing RECON count
4. Wait 3-4 seconds, then press SELECT to stop

**Expected:** The screen shows RECON incrementing with each cycle. On stop, the attack ends and MAC restores.

#### Test 3: Continuous Starvation

1. Set cooldown to 500ms (default)
2. Start starvation
3. Let it run — observe RECON count and FAIL count
4. Check "Status" for full details

#### Test 4: Pool Exhaustion Detection

1. Start starvation on a network with a small DHCP pool
2. Eventually FAIL count reaches 10+
3. OLED shows `FULL: YES`
4. Status page shows `POOL FULL?: YES`

#### Test 5: Clean Restoration

1. Stop the attack
2. Verify the ESP's MAC is restored (check Status → MAC matches hardware)

### 6.4 Router/AP Observation

On the router's admin interface, check:
1. **DHCP Client List** — Many clients with `02:` starting MACs
2. **DHCP Pool Usage** — Near 100%
3. **System Log** — May show "DHCP pool exhausted" warnings

---

## 7. ESP32 Hardware Limitations

### 7.1 MAC Address Restrictions

`esp_wifi_set_mac()` changes the runtime MAC but:
- The eFuse MAC is still the "real" MAC
- Some APs can detect the original MAC through Wi-Fi association
- Changing STA MAC does not affect AP or Bluetooth MAC

### 7.2 Wi-Fi Stack Overhead

Each reconnection cycle involves: disconnecting → changing MAC → scanning → authenticating (WPA2 4-way handshake) → DHCP DORA. This takes **1-5 seconds** depending on signal strength and AP responsiveness.

### 7.3 Connection State Tracking

After many rapid cycles, the ESP32's Wi-Fi stack can become inconsistent. The 100ms delay between disconnect and reconnect helps settle the internal state machine.

### 7.4 SRAM Fragmentation

Wi-Fi stack allocations cause heap fragmentation over time. After 100 cycles, a full Wi-Fi stack restart (`esp_wifi_stop()` / `esp_wifi_start()`) mitigates this.

---

## 8. Limitations

| Limitation | Reason | Impact |
|------------|--------|--------|
| **Rate limited to 1-2 IPs/s** | ESP32 processing + AP overhead | Takes ~2 min to exhaust a /24 pool |
| **No collision detection** | Random MAC generation | Tiny chance of duplicate MAC |
| **AP rate limiting** | AP firmware defense | Attack slows after ~100 leases |
| **No multi-AP support** | One radio | Can only attack one network at a time |

---

## 9. Defensive Countermeasures

1. **DHCP Snooping** — Enterprise switches filter DHCP packets from untrusted ports
2. **Port Security** — Limit number of MAC addresses allowed per switch port
3. **MAC Filtering** — Block locally administered addresses (`02:x:x:x:x:x`)
4. **Rate Limiting** — Limit DHCP requests per second from a single port
5. **802.1X** — Port-based authentication ensures only authorized devices connect
6. **Larger DHCP Pool** — More addresses = longer time to exhaust (mitigation)

---

## 10. Files Reference

| File | Purpose |
|------|---------|
| `platformio.ini` | Build configuration for ESP32 |
| `src/main.cpp` | Entry point, FreeRTOS task, module wiring |
| `src/config.{h,cpp}` | AppConfig + NVS + version hash auto-reset |
| `src/dhcp_starvation.{h,cpp}` | Starvation engine (module API) |
| `src/serial_cli.{h,cpp}` | Serial CLI fallback |
| `src/wifi_manager.{h,cpp}` | Wi-Fi connection management |
| `src/display_manager.{h,cpp}` | SSD1306 drawing helpers |
| `src/buttons.{h,cpp}` | GPIO button handling |
| `src/ui_menu.{h,cpp}` | OLED menu system |

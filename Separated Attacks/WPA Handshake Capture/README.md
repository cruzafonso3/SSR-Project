# WPA Handshake Capture — Sniffer & Cracker (ESP32 + Python)

Educational WPA/WPA2 4-way handshake capture tool. The ESP2 sniffs EAPOL key frames in promiscuous mode, saves the handshake to SPIFFS, and companion Python scripts extract and crack the password.

**Note:** This project uses a serial CLI only (no OLED/buttons — the ESP32 is headless, operated entirely over USB serial).

## How It Works

### On the ESP32
1. **Promiscuous mode** — Captures all 802.11 frames on the configured channel.
2. **EAPOL detection** — Filters for frames with EtherType `0x888E` (EAPOL).
3. **Message classification** — Parses the Key Information field to identify M1, M2, M3, M4 of the 4-way handshake.
4. **Storage** — Stores ANonce, SNonce, MICs, and raw EAPOL frames in memory. A complete handshake requires at least M1 + M2.
5. **Persistence** — Saves/loads captures to/from SPIFFS (`/handshake.bin`).

### On the Host (Python scripts in `tools/`)
| Script | Purpose |
|--------|---------|
| `capture_handshake.py` | Fetches binary dump from ESP32 over serial |
| `convert_handshake.py` | Converts to hashcat mode 22000 format |
| `crack_handshake.py` | Pure-Python WPA cracker (PBKDF2-SHA1, PRF-512, HMAC-SHA1 MIC check) |

Wordlists: `tools/wordlists/tiny.txt`, `common.txt`, `lab.txt`

## Commands

```
  set_ssid <name>                  Target AP SSID
  set_bssid <AA:BB:CC:DD:EE:FF>   Target AP BSSID
  set_channel <1-14>               WiFi channel to sniff
  start                            Begin sniffing
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

## Build

```bash
pio run
pio run --target upload
```

Requires SPIFFS partition (configured in `platformio.ini`: `board_build.filesystem = spiffs`).

## Technical Notes

- The sniffer captures WPA2-Personal (RSN) key handshakes only.
- M1+M2 is sufficient for offline cracking; M3/M4 provide additional MIC verification.
- Uses the `ieee80211_raw_frame_sanity_check` bypass for raw promiscuous compatibility.
- EAPOL frame parsing handles both data and QoS data frame subtypes.
- BSSID filtering is recommended to avoid capturing handshakes from unrelated APs.

> **Authorized lab use only.** Do not use on networks you do not own.

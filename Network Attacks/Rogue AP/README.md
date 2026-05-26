# Rogue AP — Evil Twin + Captive Portal (ESP32)

Educational Evil Twin / Rogue Access Point attack tool for the ESP32. Creates a cloned version of a target Wi-Fi network, runs a DNS hijack that redirects all traffic to a captive portal, and captures credentials submitted by victims.

**No OLED, no buttons, no hardware peripherals needed** — the entire interface is a web-based admin panel served over the ESP32's own Wi-Fi network.

## How It Works

### Phase 1 — Admin Mode

The ESP32 boots its own Wi-Fi network:

| Field | Default |
|-------|---------|
| SSID | `RogueAP` |
| Password | `password123` |
| IP | `192.168.4.1` |

Connect to this network and open `http://192.168.4.1/admin` to access the admin panel.

### Phase 2 — Target Selection

In the admin panel:
1. **Scan** for nearby Wi-Fi networks
2. **Select** a target from the list
3. Optionally enter the target's Wi-Fi password (if known, for a WPA2 clone)
4. **Start** the attack

### Phase 3 — Evil Twin Active

The ESP32:
1. Shuts down the admin AP
2. Creates a new AP **cloning the target's SSID and channel**
3. If a password was provided → WPA2-protected clone; otherwise open network
4. **Captive portal** serves phishing pages to steal credentials

When a victim connects and tries to browse, they get a fake login/registration page. Submitted credentials are saved to SPIFFS and viewable in the admin panel.

## Admin Web UI

Access at `http://192.168.4.1/admin`:

| Feature | Description |
|---------|-------------|
| **Status** | Live badge with connected client count; auto-refresh every 5s when active |
| **Scan** | Lists nearby networks (SSID, channel, RSSI) up to 20 results |
| **Select Target** | Click a network → enter password (optional) → confirm |
| **Start/Stop** | Toggle the rogue AP on/off |
| **Config** | Edit page title, subtitle, body text; select custom HTML template |
| **Upload HTML** | Upload custom captive portal pages to SPIFFS |
| **Captured Data** | View all stolen credentials with Clear button |
| **Export CSV** | Download credentials as CSV |
| **Auto-Stop** | Set timer (5/15/30/60/120 min) to auto-disable the attack |

<img width="967" height="509" alt="image" src="https://github.com/user-attachments/assets/aea2645e-9ec0-4f0e-9590-2637d8bb2364" />


## Captive Portal Templates (3 built-in)

| Template | Description |
|----------|-------------|
| **Firmware Update** | Simple single-field WiFi password prompt |
| **Free WiFi Register** | Name, email, phone, and password fields |
| **ZON ISP Verify** | Branded ZON (Portuguese ISP) login page |

The portal served depends on whether a target password was provided:
- **With password** → ZON ISP Verify (realistic phish for WPA2)
<img width="403" height="652" alt="image" src="https://github.com/user-attachments/assets/f606ff33-d648-47a2-bc3d-c4ba80d14f6d" />

- **No password (open)** → Free WiFi Register
<img width="407" height="643" alt="image" src="https://github.com/user-attachments/assets/f9df437f-4f9e-431e-9392-0777527148e7" />

Custom HTML pages can be uploaded via the admin panel and selected in Config.

## Build

```bash
pio run
pio run --target upload
```

The build process automatically runs `tools/embed_html.py` as a pre-build step, converting the `html/` templates into `src/generated_htmls.h`.

## Configuration

All defaults in `src/config.h`:

| Define | Default | Notes |
|--------|---------|-------|
| `AP_IP_ADDR` | `192.168.4.1` | Admin AP / rogue AP gateway |
| `DEFAULT_AP_SSID` | `"RogueAP"` | Admin network name |
| `DEFAULT_AP_PASS` | `"password123"` | Admin network password |
| `DNS_PORT` | 53 | DNS hijack listener |
| `SCAN_INTERVAL_MS` | 15000 | Min interval between scans |
| `WIFI_COUNTRY_CC` | `"CN"` | Country code for channel range |

## SPIFFS Persistence

| File | Contents |
|------|----------|
| `/creds.txt` | Captured credentials (timestamp, SSID, fields, client IP) |
| `/ap_config.json` | Attack profile (page text, auto-stop settings, custom HTML selection) |
| `/custom/*.html` | Uploaded custom captive portal pages |


## Routes

| Path | Purpose |
|------|---------|
| `/admin` | Admin panel |
| `/control` | Start/stop rogue AP |
| `/scan` | Scan nearby networks |
| `/select` | Select target BSSID + password |
| `/config` | Edit attack page text, select custom HTML |
| `/upload` | Upload custom HTML templates |
| `/export` | Download credentials CSV |
| `/clear-logs` | Wipe credential log |
| `/` | Captive portal (when rogue AP active) |

## Technical Notes

- Uses `DNSServer` library for DNS hijack (port 53, wildcard `"*"` → `192.168.4.1`)
- Uses `WebServer` library for HTTP (port 80)
- Captive portal detection bypasses: all unknown routes redirect to the portal
- Auto-stop timer resets the ESP32's AP + DNS state when it expires
- No JSON library used — config is parsed manually from SPIFFS
- Country code set to `"CN"` (channels 1-13); change in `config.h` if needed

> **Authorized lab use only.** Do not use on networks you do not own.

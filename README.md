# SSR-Project — Exploring the ESP32 as an Attack Platform

Educational cybersecurity lab demonstrating various Wi-Fi attacks using the ESP32 microcontrroller. Each attack is a standalone PlatformIO project in its own folder under `Network Attacks/`.

## Attacks

| Folder | Attack | Interface |
|--------|--------|-----------|
| `Network Attacks/ARP SPOOF/` | ARP cache poisoning MITM | OLED 128x64 + buttons |
| `Network Attacks/DHCP Starvation/` | DHCP pool exhaustion | OLED 128x64 + buttons |
| `Network Attacks/WPA Handshake Capture/` | 4-way handshake sniffing + cracking | Serial CLI + Python tools |
| `Network Attacks/Rogue AP/` | Evil Twin + captive portal + credential theft | Web admin UI |
| `Network Attacks/Deauthentication/` | Deauth DoS attack | Serial CLI |

## Common Hardware

All projects use the **ESP32 DOIT DevKit V1** board.

Projects with OLED + buttons use this wiring:

| Component | Pin |
|-----------|-----|
| OLED SSD1306 SDA | GPIO 21 |
| OLED SSD1306 SCL | GPIO 22 |
| Button UP | GPIO 14 (internal pullup, GND) |
| Button DOWN | GPIO 27 (internal pullup, GND) |
| Button SELECT | GPIO 26 (internal pullup, GND) |

## Build

Each attack folder is a self-contained PlatformIO project:

```bash
cd "Network Attacks/<Attack Folder>"
pio run
pio run --target upload
```

## Documentation

- Full project proposal: `Documentation/Exploring_the_ESP32_as_an_Attack_Platform_project_proposal.pdf`
- Each attack folder has its own `README.md` with detailed setup and usage instructions

> **For authorized lab use only.** Do not use on networks you do not own.

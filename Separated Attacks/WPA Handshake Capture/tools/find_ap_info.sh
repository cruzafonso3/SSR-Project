#!/bin/bash
# find_ap_info.sh
# Run this on your laptop while connected to the target hotspot/AP.
# It automatically finds the SSID, BSSID (AP MAC), Channel, and your laptop's MAC (STA MAC).

echo "[*] Detecting wireless interface..."
IFACE=$(ip route show default | awk '{print $5}' | head -n1)

# Verify it's actually a wireless interface
if ! iw dev "$IFACE" info >/dev/null 2>&1; then
    echo "[!] Error: Could not find a wireless interface, or $IFACE is not Wi-Fi."
    echo "[!] Make sure you are connected to the target hotspot via Wi-Fi."
    exit 1
fi

echo "[*] Interface: $IFACE"

# Get current connection info from iw
LINK_INFO=$(iw dev "$IFACE" link 2>/dev/null)

if [ -z "$LINK_INFO" ]; then
    echo "[!] Error: Not connected to any access point."
    exit 1
fi

SSID=$(echo "$LINK_INFO" | grep "SSID:" | sed 's/.*SSID: //')
BSSID=$(echo "$LINK_INFO" | grep "Connected to" | awk '{print $3}')

# Try multiple methods to get the channel
CHANNEL=$(iw dev "$IFACE" info 2>/dev/null | grep -oP 'channel \K[0-9]+')
if [ -z "$CHANNEL" ]; then
    CHANNEL=$(echo "$LINK_INFO" | grep -oP 'channel \K[0-9]+')
fi
if [ -z "$CHANNEL" ]; then
    # Fallback: nmcli
    if command -v nmcli &> /dev/null; then
        CHANNEL=$(nmcli -t -f active,ssid,chan dev wifi | grep "^yes:" | grep ":$SSID:" | awk -F: '{print $3}')
    fi
fi

STAMAC=$(ip link show "$IFACE" | grep -oP 'link/ether \K[0-9a-f:]{17}')

echo ""
echo "========================================"
echo "  WPA Handshake Capture - Target Info"
echo "========================================"
echo ""
echo "--- ACCESS POINT (Hotspot/Router) ---"
echo "SSID:       $SSID"
echo "BSSID:      $BSSID"
echo "Channel:    $CHANNEL"
echo ""
echo "--- CLIENT (This Laptop) ---"
echo "STA MAC:    $STAMAC"
echo "========================================"
echo ""
echo "Copy these values into the ESP32 Serial Monitor:"
echo ""
echo "> set_ssid $SSID"
echo "> set_bssid $BSSID"
echo "> set_channel $CHANNEL"
echo "> set_stamac $STAMAC"
echo ""
echo "> start"
echo "> deauth_start"

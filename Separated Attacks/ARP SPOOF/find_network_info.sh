#!/bin/bash
# find_network_info.sh
# Automatically finds network interface, target IP/MAC, gateway IP/MAC
# Run this on the target laptop while connected to the Wi-Fi network.

IFACE=$(ip route show default | awk '{print $5}' | head -n1)

TARGET_IP=$(ip addr show "$IFACE" | grep -oP 'inet \K[0-9.]+' | head -n1)
TARGET_MAC=$(ip addr show "$IFACE" | grep -oP 'link/ether \K[0-9a-f:]{17}' | head -n1)

GATEWAY_IP=$(ip route show default | grep "$IFACE" | awk '{print $3}')

# Primary method: get Gateway MAC (BSSID) from Wi-Fi link info.
# The AP the laptop is connected to IS the gateway, so its BSSID is the gateway MAC.
GATEWAY_MAC=$(iw dev "$IFACE" link 2>/dev/null | grep "Connected to" | awk '{print $3}')

# Fallback: try ARP table if iw is not available or interface is not wireless.
if [ -z "$GATEWAY_MAC" ]; then
    GATEWAY_MAC=$(ip neigh show | grep "$GATEWAY_IP" | awk '{print $3}')
fi

echo "========================================"
echo "  Network Information"
echo "========================================"
echo "Interface:   $IFACE"
echo ""
echo "--- TARGET (This Laptop) ---"
echo "Target IP:   $TARGET_IP"
echo "Target MAC:  $TARGET_MAC"
echo ""
echo "--- GATEWAY (Access Point) ---"
echo "Gateway IP:  $GATEWAY_IP"
echo "Gateway MAC: $GATEWAY_MAC"
echo "========================================"
echo ""
echo "Copy these values into the ESP32 Serial Monitor:"
echo ""
echo "> set_target_ip $TARGET_IP"
echo "> set_target_mac $TARGET_MAC"
echo "> set_gateway_ip $GATEWAY_IP"
echo "> set_gateway_mac $GATEWAY_MAC"

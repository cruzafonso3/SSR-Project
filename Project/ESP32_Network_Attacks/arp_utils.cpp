#include "arp_utils.h"
#include "packet_utils.h"

extern uint8_t g_esp32_mac[6];
extern uint8_t g_bssid[6];

void sendArpReply(const uint8_t* targetMac, IPAddress spoofIp,
                  const uint8_t* spoofMac, IPAddress targetIp) {
    uint8_t arp[28];
    arp[0] = 0x00; arp[1] = 0x01;
    arp[2] = 0x08; arp[3] = 0x00;
    arp[4] = 0x06; arp[5] = 0x04;
    arp[6] = 0x00; arp[7] = 0x02;
    memcpy(&arp[8], spoofMac, 6);
    arp[14] = spoofIp[0]; arp[15] = spoofIp[1];
    arp[16] = spoofIp[2]; arp[17] = spoofIp[3];
    memcpy(&arp[18], targetMac, 6);
    arp[24] = targetIp[0]; arp[25] = targetIp[1];
    arp[26] = targetIp[2]; arp[27] = targetIp[3];
    inject80211(targetMac, g_esp32_mac, g_bssid, LLC_SNAP_ARP, arp, 28);
}

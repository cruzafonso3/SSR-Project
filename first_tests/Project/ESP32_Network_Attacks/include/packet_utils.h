#ifndef PACKET_UTILS_H
#define PACKET_UTILS_H

#include <Arduino.h>
#include <stdint.h>

#define DOT11_DATA_HDR_LEN 24
#define LLC_SNAP_LEN 8

extern const uint8_t LLC_SNAP_IP[8];
extern const uint8_t LLC_SNAP_ARP[8];

bool inject80211(const uint8_t* addr3_dest, const uint8_t* addr2_src,
                 const uint8_t* addr1_bssid, const uint8_t* llc_snap,
                 const uint8_t* payload, uint16_t payload_len);

uint16_t ipChecksum(const uint8_t* data, uint16_t len);
void printMAC(const uint8_t* mac);

#endif

#ifndef DHCP_UTILS_H
#define DHCP_UTILS_H

#include <Arduino.h>

void dhcpStart();
void dhcpStop();
void dhcpProcessPacket(const uint8_t* ipPkt, const uint8_t* udpPkt, const uint8_t* dhcpData, uint16_t dhcpLen);

#endif
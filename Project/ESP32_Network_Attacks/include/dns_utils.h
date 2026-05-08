#ifndef DNS_UTILS_H
#define DNS_UTILS_H

#include <Arduino.h>
#include <stdint.h>

bool parseDnsQueryDomain(const uint8_t* dnsData, uint16_t dnsLen,
                         char* out, uint8_t outLen);

void sendDnsResponse(const uint8_t* originalIpPkt, const uint8_t* originalUdpPkt,
                     const uint8_t* dnsData, uint16_t dnsLen,
                     const uint8_t* victimMac, const uint8_t* ourMac,
                     const uint8_t* bssid);

#endif

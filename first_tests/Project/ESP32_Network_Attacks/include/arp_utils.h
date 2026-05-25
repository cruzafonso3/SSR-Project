#ifndef ARP_UTILS_H
#define ARP_UTILS_H

#include <Arduino.h>
#include <stdint.h>

void sendArpReply(const uint8_t* targetMac, IPAddress spoofIp,
                  const uint8_t* spoofMac, IPAddress targetIp);

#endif

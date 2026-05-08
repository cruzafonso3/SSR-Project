#ifndef FORWARDING_H
#define FORWARDING_H

#include <Arduino.h>
#include <stdint.h>

void forwardToGateway(const uint8_t* ipPkt, uint16_t ipLen);
void forwardToVictim(const uint8_t* ipPkt, uint16_t ipLen);

#endif
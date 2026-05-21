#ifndef ROGUE_AP_DNS_CORE_H
#define ROGUE_AP_DNS_CORE_H

#include <Arduino.h>
#include <DNSServer.h>
#include "config.h"

void initDnsHijack(DNSServer& server, uint16_t port, IPAddress ip);
void stopDnsHijack(DNSServer& server);
void processDnsRequests(DNSServer& server);

#endif

#include "dns_core.h"

void initDnsHijack(DNSServer& server, uint16_t port, IPAddress ip) {
    server.stop();
    server.start(port, "*", ip);
    Serial.println("[INFO] DNS hijack active - all domains redirected");
}

void stopDnsHijack(DNSServer& server) {
    server.stop();
}

void processDnsRequests(DNSServer& server) {
    server.processNextRequest();
}

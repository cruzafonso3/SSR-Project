#include "settings_manager.h"
#include <Preferences.h>

Preferences prefs;

char g_wifi_ssid[MAX_SSID_LEN + 1];
char g_wifi_pass[MAX_PASS_LEN + 1];
uint8_t g_victim_mac[6];
IPAddress g_victim_ip;
uint8_t g_gateway_mac[6];
IPAddress g_gateway_ip;
char g_target_domain[MAX_DOMAIN_LEN + 1];
IPAddress g_spoofed_ip;
int g_arp_interval_ms;

static void loadString(const char* key, char* dest, size_t maxLen, const char* defaultVal) {
    String s = prefs.getString(key, defaultVal);
    strlcpy(dest, s.c_str(), maxLen);
}

static void loadIp(const char* key, IPAddress& dest, const char* defaultVal) {
    String s = prefs.getString(key, defaultVal);
    dest.fromString(s.c_str());
}

static void loadMac(const char* key, uint8_t* dest, const char* defaultVal) {
    String s = prefs.getString(key, defaultVal);
    stringToMac(s.c_str(), dest);
}

void settingsInit() {
    prefs.begin("netattack", false);
    
    // If code was re-flashed with new defaults, force a reset so the user sees them
    int storedVer = prefs.getInt("version", 0);
    if (storedVer != SETTINGS_VERSION) {
        settingsReset();
        prefs.putInt("version", SETTINGS_VERSION);
        settingsSave();
        return;
    }
    
    loadString("wifi_ssid", g_wifi_ssid, sizeof(g_wifi_ssid), DEFAULT_WIFI_SSID);
    loadString("wifi_pass", g_wifi_pass, sizeof(g_wifi_pass), DEFAULT_WIFI_PASS);
    
    loadMac("victim_mac", g_victim_mac, DEFAULT_VICTIM_MAC);
    loadIp("victim_ip", g_victim_ip, DEFAULT_VICTIM_IP);
    
    loadMac("gw_mac", g_gateway_mac, DEFAULT_GATEWAY_MAC);
    loadIp("gw_ip", g_gateway_ip, DEFAULT_GATEWAY_IP);
    
    loadString("domain", g_target_domain, sizeof(g_target_domain), DEFAULT_TARGET_DOMAIN);
    loadIp("spoof_ip", g_spoofed_ip, DEFAULT_SPOOFED_IP);
    
    g_arp_interval_ms = prefs.getInt("arp_int", DEFAULT_ARP_INTERVAL);
    if (g_arp_interval_ms < 100) g_arp_interval_ms = 100;
    if (g_arp_interval_ms > 30000) g_arp_interval_ms = 30000;
}

void settingsSave() {
    char buf[32];
    
    prefs.putString("wifi_ssid", g_wifi_ssid);
    prefs.putString("wifi_pass", g_wifi_pass);
    
    macToString(g_victim_mac, buf, sizeof(buf));
    prefs.putString("victim_mac", buf);
    prefs.putString("victim_ip", g_victim_ip.toString().c_str());
    
    macToString(g_gateway_mac, buf, sizeof(buf));
    prefs.putString("gw_mac", buf);
    prefs.putString("gw_ip", g_gateway_ip.toString().c_str());
    
    prefs.putString("domain", g_target_domain);
    prefs.putString("spoof_ip", g_spoofed_ip.toString().c_str());
    prefs.putInt("arp_int", g_arp_interval_ms);
}

void settingsReset() {
    prefs.clear();
    
    strlcpy(g_wifi_ssid, DEFAULT_WIFI_SSID, sizeof(g_wifi_ssid));
    strlcpy(g_wifi_pass, DEFAULT_WIFI_PASS, sizeof(g_wifi_pass));
    stringToMac(DEFAULT_VICTIM_MAC, g_victim_mac);
    g_victim_ip.fromString(DEFAULT_VICTIM_IP);
    stringToMac(DEFAULT_GATEWAY_MAC, g_gateway_mac);
    g_gateway_ip.fromString(DEFAULT_GATEWAY_IP);
    strlcpy(g_target_domain, DEFAULT_TARGET_DOMAIN, sizeof(g_target_domain));
    g_spoofed_ip.fromString(DEFAULT_SPOOFED_IP);
    g_arp_interval_ms = DEFAULT_ARP_INTERVAL;
}

void ipToString(const IPAddress& ip, char* buf, size_t len) {
    snprintf(buf, len, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

bool stringToIp(const char* str, IPAddress& ip) {
    return ip.fromString(str);
}

void macToString(const uint8_t* mac, char* buf, size_t len) {
    snprintf(buf, len, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool stringToMac(const char* str, uint8_t* mac) {
    int values[6];
    if (sscanf(str, "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == 6) {
        for (int i = 0; i < 6; i++) mac[i] = (uint8_t)values[i];
        return true;
    }
    if (sscanf(str, "%2x%2x%2x%2x%2x%2x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == 6) {
        for (int i = 0; i < 6; i++) mac[i] = (uint8_t)values[i];
        return true;
    }
    return false;
}

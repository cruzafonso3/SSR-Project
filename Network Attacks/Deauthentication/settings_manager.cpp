#include "settings_manager.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

Settings g_settings;

static void parseMac(const char* str, uint8_t* mac) {
    for (int i = 0; i < 6; i++) {
        mac[i] = (uint8_t)strtol(str + i * 3, NULL, 16);
    }
}

static String macToString(uint8_t* mac) {
    char buf[18];
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

void settingsInit() {
    settingsResetDefaults();
}

void settingsResetDefaults() {
    strlcpy(g_settings.wifi_ssid, DEF_WIFI_SSID, sizeof(g_settings.wifi_ssid));
    strlcpy(g_settings.wifi_pass, DEF_WIFI_PASS, sizeof(g_settings.wifi_pass));
    strlcpy(g_settings.target_domain, DEF_TARGET_DOMAIN, sizeof(g_settings.target_domain));
    g_settings.spoofed_ip.fromString(DEF_SPOOFED_IP);
    g_settings.victim_ip.fromString(DEF_VICTIM_IP);
    parseMac(DEF_VICTIM_MAC, g_settings.victim_mac);
    g_settings.gateway_ip.fromString(DEF_GATEWAY_IP);
    parseMac(DEF_GATEWAY_MAC, g_settings.gateway_mac);
    g_settings.arp_interval = DEF_ARP_INTERVAL;
    g_settings.deauth_burst = DEF_DEAUTH_BURST;
    strlcpy(g_settings.rogue_ap_ssid, DEF_ROGUE_SSID, sizeof(g_settings.rogue_ap_ssid));
}

bool settingsLoad() {
    if (!SPIFFS.exists("/config.json")) return false;
    File f = SPIFFS.open("/config.json", "r");
    if (!f) return false;
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;
    strlcpy(g_settings.wifi_ssid, doc["wifi_ssid"] | DEF_WIFI_SSID, sizeof(g_settings.wifi_ssid));
    strlcpy(g_settings.wifi_pass, doc["wifi_pass"] | DEF_WIFI_PASS, sizeof(g_settings.wifi_pass));
    strlcpy(g_settings.target_domain, doc["target_domain"] | DEF_TARGET_DOMAIN, sizeof(g_settings.target_domain));
    g_settings.spoofed_ip.fromString(doc["spoofed_ip"] | DEF_SPOOFED_IP);
    g_settings.victim_ip.fromString(doc["victim_ip"] | DEF_VICTIM_IP);
    parseMac(doc["victim_mac"] | DEF_VICTIM_MAC, g_settings.victim_mac);
    g_settings.gateway_ip.fromString(doc["gateway_ip"] | DEF_GATEWAY_IP);
    parseMac(doc["gateway_mac"] | DEF_GATEWAY_MAC, g_settings.gateway_mac);
    g_settings.arp_interval = doc["arp_interval"] | DEF_ARP_INTERVAL;
    g_settings.deauth_burst = doc["deauth_burst"] | DEF_DEAUTH_BURST;
    strlcpy(g_settings.rogue_ap_ssid, doc["rogue_ap_ssid"] | DEF_ROGUE_SSID, sizeof(g_settings.rogue_ap_ssid));
    return true;
}

bool settingsSave() {
    StaticJsonDocument<1024> doc;
    doc["wifi_ssid"] = g_settings.wifi_ssid;
    doc["wifi_pass"] = g_settings.wifi_pass;
    doc["target_domain"] = g_settings.target_domain;
    doc["spoofed_ip"] = g_settings.spoofed_ip.toString();
    doc["victim_ip"] = g_settings.victim_ip.toString();
    doc["victim_mac"] = macToString(g_settings.victim_mac);
    doc["gateway_ip"] = g_settings.gateway_ip.toString();
    doc["gateway_mac"] = macToString(g_settings.gateway_mac);
    doc["arp_interval"] = g_settings.arp_interval;
    doc["deauth_burst"] = g_settings.deauth_burst;
    doc["rogue_ap_ssid"] = g_settings.rogue_ap_ssid;
    File f = SPIFFS.open("/config.json", "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

void settingsApplyToGlobals() {
    extern uint8_t VICTIM_MAC[6];
    extern IPAddress VICTIM_IP;
    extern uint8_t GATEWAY_MAC[6];
    extern IPAddress GATEWAY_IP;
    extern char TARGET_DOMAIN_STR[];
    extern IPAddress SPOOFED_IP;
    memcpy(VICTIM_MAC, g_settings.victim_mac, 6);
    VICTIM_IP = g_settings.victim_ip;
    memcpy(GATEWAY_MAC, g_settings.gateway_mac, 6);
    GATEWAY_IP = g_settings.gateway_ip;
    strlcpy(TARGET_DOMAIN_STR, g_settings.target_domain, 64);
    SPOOFED_IP = g_settings.spoofed_ip;
}

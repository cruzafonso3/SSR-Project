#include "wifi_core.h"
#include "utils.h"

void configureCountryRegion() {
    wifi_country_t country = {
        .cc = WIFI_COUNTRY_CC,
        .schan = WIFI_COUNTRY_SCHAN,
        .nchan = WIFI_COUNTRY_NCHAN,
        .policy = WIFI_COUNTRY_POLICY_AUTO,
    };
    esp_wifi_set_country(&country);
}

void scanNearbyNetworks(std::vector<NetworkRecord>& results) {
    results.clear();
    int count = WiFi.scanNetworks(false, true);
    if (count > 0) {
        int limit = (count < MAX_SCAN_RESULTS) ? count : MAX_SCAN_RESULTS;
        for (int i = 0; i < limit; i++) {
            NetworkRecord net;
            net.ssid = WiFi.SSID(i);
            if (net.ssid.isEmpty()) net.ssid = "[Hidden]";
            memcpy(net.bssid, WiFi.BSSID(i), 6);
            net.channel = (uint8_t)WiFi.channel(i);
            net.bssidString = macAddressToString(net.bssid, 6);
            results.push_back(net);
        }
    }
    WiFi.scanDelete();
}

bool launchRogueAP(const NetworkRecord& target, const String& password, IPAddress apIp) {
    if (target.ssid.isEmpty()) return false;

    WiFi.softAPdisconnect(true);
    delay(AP_SWITCH_DELAY_MS);

    WiFi.softAPConfig(apIp, apIp, AP_SUBNET);

    bool success;
    if (password.length() >= 8) {
        success = WiFi.softAP(target.ssid.c_str(), password.c_str(), target.channel);
        Serial.println("[INFO] Rogue AP started (WPA2): " + target.ssid);
    } else {
        success = WiFi.softAP(target.ssid.c_str(), NULL, target.channel);
        Serial.println("[INFO] Rogue AP started (Open): " + target.ssid);
    }
    if (success) {
        Serial.println("[INFO] Waiting for connections...");
    } else {
        Serial.println("[ERROR] Failed to start rogue AP, falling back to default");
        WiFi.softAP(getDefaultSSID(), DEFAULT_AP_PASS);
    }
    return success;
}

void haltRogueAP() {
    WiFi.softAPdisconnect(true);
    delay(AP_SWITCH_DELAY_MS);
    WiFi.softAP(getDefaultSSID(), DEFAULT_AP_PASS);
    Serial.println("[INFO] Rogue AP stopped, returned to default");
}

uint8_t getConnectedClientCount() {
    return WiFi.softAPgetStationNum();
}

const char* getDefaultSSID() {
    return DEFAULT_AP_SSID;
}

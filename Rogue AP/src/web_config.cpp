#include "web_config.h"
#include "storage.h"
#include "utils.h"
#include "generated_htmls.h"

void serveConfigPage(WebServer& server, const AttackProfile& config) {
    String html = String(FPSTR(ADMIN_HEADER_HTML));
    html.replace("%TITLE%", "Config");

    html += "<div class='card'><h2>Attack Page</h2><form method='post' action='/save-config'>";
    html += "<label>Title:</label><input type='text' name='title' value='" + config.pageTitle + "'>";
    html += "<label>Subtitle:</label><input type='text' name='subtitle' value='" + config.pageSubtitle + "'>";
    html += "<label>Body:</label><textarea name='body' rows='3'>" + config.pageBody + "</textarea>";

    html += "<label>Select HTML File:</label><select name='html_file'>";
    html += "<option value=''>Default</option>";

    std::vector<String> files = listHtmlTemplates();
    for (const auto& f : files) {
        String selected = (config.customHtmlFile == f) ? "selected" : "";
        html += "<option value='" + f + "' " + selected + ">" + f + "</option>";
    }
    html += "</select>";

    html += "<h3 style='margin-top:20px;'>Auto-Stop Timer</h3>";
    html += "<label><input type='checkbox' name='auto_stop_enabled' " +
            String(config.autoStopEnabled ? "checked" : "") +
            "> Enable auto-stop</label><br><br>";

    html += "<label>Duration:</label><select name='auto_stop_minutes'>";
    const uint16_t presets[] = {5, 15, 30, 60, 120};
    for (int i = 0; i < 5; i++) {
        String selected = (config.autoStopMinutes == presets[i]) ? "selected" : "";
        html += "<option value='" + String(presets[i]) + "' " + selected + ">";
        if (presets[i] < 60) {
            html += String(presets[i]) + " minutes";
        } else {
            html += String(presets[i] / 60) + " hour" + (presets[i] >= 120 ? "s" : "");
        }
        html += "</option>";
    }
    html += "</select>";

    html += "<br><br><button class='btn'>Save</button></form>";
    html += "<br><a href='/admin' class='btn secondary'>Back</a></div>";
    html += FPSTR(FOOTER_HTML);
    server.send(200, "text/html", html);
}

void applyConfigChanges(WebServer& server, AttackProfile& config) {
    if (server.hasArg("title")) config.pageTitle = server.arg("title");
    if (server.hasArg("subtitle")) config.pageSubtitle = server.arg("subtitle");
    if (server.hasArg("body")) config.pageBody = server.arg("body");

    String selectedFile = server.arg("html_file");
    if (selectedFile.isEmpty()) {
        config.useCustomHtml = false;
        config.customHtmlFile = "";
    } else {
        config.useCustomHtml = true;
        config.customHtmlFile = selectedFile;
    }

    config.autoStopEnabled = server.hasArg("auto_stop_enabled");

    String minutesStr = server.arg("auto_stop_minutes");
    if (!minutesStr.isEmpty()) {
        config.autoStopMinutes = (uint16_t)minutesStr.toInt();
    }

    saveAttackConfig(config);

    server.sendHeader("Location", "/admin");
    server.send(302, "text/plain", "");
}

void serveUploadPage(WebServer& server) {
    String html = String(FPSTR(ADMIN_HEADER_HTML));
    html.replace("%TITLE%", "Upload");

    html += "<div class='card'><h2>Upload HTML</h2>";
    html += "<form method='post' action='/save-html'>";
    html += "<input type='text' name='filename' placeholder='filename.html'>";
    html += "<textarea name='content' rows='10'></textarea>";
    html += "<button class='btn'>Save</button></form>";
    html += "<br><a href='/admin' class='btn secondary'>Back</a></div>";
    html += FPSTR(FOOTER_HTML);
    server.send(200, "text/html", html);
}

void applyHtmlUpload(WebServer& server) {
    if (server.hasArg("filename") && server.hasArg("content")) {
        saveHtmlTemplate(server.arg("filename"), server.arg("content"));
    }
    server.sendHeader("Location", "/config");
    server.send(302, "text/plain", "");
}

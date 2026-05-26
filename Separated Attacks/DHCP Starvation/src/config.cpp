#include "config.h"
#include <Preferences.h>

AppConfig g_config = {0};
static Preferences prefs;

static int buildVersionHash() {
    const char* s = __DATE__ __TIME__;
    int h = 5381;
    for (int i = 0; s[i]; ++i) h = ((h << 5) + h) + s[i];
    return h;
}

void config_init() {
    prefs.begin(CONFIG_NAMESPACE, false);
    int storedVer = prefs.getInt("version", 0);
    int currentVer = buildVersionHash();
    if (storedVer != currentVer) {
        config_reset();
        prefs.putInt("version", currentVer);
        prefs.putString("ssid", g_config.ssid);
        prefs.putString("pass", g_config.password);
        prefs.putInt("cooldown", g_config.cooldown_ms);
        prefs.end();
        Serial.println("[Config] Factory reset (new firmware)");
        return;
    }
    prefs.end();
    config_load();
}

bool config_save() {
    prefs.begin(CONFIG_NAMESPACE, false);
    prefs.putString("ssid", g_config.ssid);
    prefs.putString("pass", g_config.password);
    prefs.putInt("cooldown", g_config.cooldown_ms);
    prefs.putInt("version", buildVersionHash());
    prefs.end();
    return true;
}

bool config_load() {
    prefs.begin(CONFIG_NAMESPACE, true);
    String s;
    s = prefs.getString("ssid", "");
    strncpy(g_config.ssid, s.c_str(), sizeof(g_config.ssid) - 1);
    s = prefs.getString("pass", "");
    strncpy(g_config.password, s.c_str(), sizeof(g_config.password) - 1);
    g_config.cooldown_ms = prefs.getInt("cooldown", 500);
    prefs.end();
    return true;
}

void config_reset() {
    memset(&g_config, 0, sizeof(g_config));
    g_config.cooldown_ms = 500;
}

bool config_validate() {
    return strlen(g_config.ssid) > 0;
}

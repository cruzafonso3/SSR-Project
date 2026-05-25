#include "serial_cli.h"
#include "wifi_manager.h"
#include "dhcp_glutton.h"
#include <Preferences.h>

static String s_input;
static Preferences prefs;

static char s_ssid[32] = "";
static char s_password[64] = "";
static int s_cooldown = 1000;

static void print_help() {
    Serial.println("\n========================================");
    Serial.println("  DHCP Starvation - Educational Demo");
    Serial.println("  FOR AUTHORIZED LAB USE ONLY");
    Serial.println("========================================\n");
    Serial.println("Commands:");
    Serial.println("  set_ssid <name>          Set SSID");
    Serial.println("  set_pass <password>      Set password");
    Serial.println("  set_cooldown <ms>        Delay between reconnects (500-10000, default 1000)");
    Serial.println("  connect                  Connect to WiFi");
    Serial.println("  start                    Begin DHCP starvation");
    Serial.println("  stop                     Stop + restore MAC");
    Serial.println("  status                   Show current stats");
    Serial.println("  save                     Save config to flash");
    Serial.println("  load                     Load config from flash");
    Serial.println("  reset                    Reset defaults");
    Serial.println("  help                     Show this help");
}

static void save_config() {
    prefs.begin("dhcpstarve", false);
    prefs.putString("ssid", s_ssid);
    prefs.putString("pass", s_password);
    prefs.putInt("cooldown", s_cooldown);
    prefs.end();
    Serial.println("Saved");
}

static void load_config() {
    prefs.begin("dhcpstarve", true);
    String s = prefs.getString("ssid", "");
    strncpy(s_ssid, s.c_str(), sizeof(s_ssid) - 1);
    s = prefs.getString("pass", "");
    strncpy(s_password, s.c_str(), sizeof(s_password) - 1);
    s_cooldown = prefs.getInt("cooldown", 1000);
    prefs.end();
    Serial.println("Loaded");
}

static void reset_config() {
    strcpy(s_ssid, "test");
    strcpy(s_password, "12345678");
    s_cooldown = 1000;
    Serial.println("Reset to defaults");
}

static void handle_command(const String& cmd) {
    if (cmd.startsWith("set_ssid ")) {
        strncpy(s_ssid, cmd.substring(9).c_str(), sizeof(s_ssid) - 1);
        Serial.printf("SSID: %s\n", s_ssid);
    } else if (cmd.startsWith("set_pass ")) {
        strncpy(s_password, cmd.substring(9).c_str(), sizeof(s_password) - 1);
        Serial.println("Password set");
    } else if (cmd.startsWith("set_cooldown ")) {
        s_cooldown = cmd.substring(13).toInt();
        if (s_cooldown < 500) s_cooldown = 500;
        if (s_cooldown > 10000) s_cooldown = 10000;
        Serial.printf("Cooldown: %d ms\n", s_cooldown);
    } else if (cmd == "connect") {
        if (strlen(s_ssid) == 0) {
            Serial.println("Error: SSID not set");
            return;
        }
        wifi_manager_connect(s_ssid, s_password);
    } else if (cmd == "start") {
        if (strlen(s_ssid) == 0) {
            Serial.println("Error: SSID not set");
            return;
        }
        if (!wifi_manager_is_connected()) {
            Serial.println("[DHCP] Auto-connecting...");
            if (!wifi_manager_connect(s_ssid, s_password)) {
                Serial.println("[DHCP] Connect failed");
                return;
            }
        }
        destroy_glutton();
        create_glutton(s_ssid, s_password, s_cooldown);
        get_glutton()->start();
    } else if (cmd == "stop") {
        DHCPGlutton* g = get_glutton();
        if (g) g->stop();
        destroy_glutton();
    } else if (cmd == "status") {
        Serial.println("--- Status ---");
        Serial.printf("SSID: %s\n", s_ssid);
        Serial.printf("WiFi: %s\n", wifi_manager_is_connected() ? "Connected" : "Disconnected");
        Serial.printf("Cooldown: %d ms\n", s_cooldown);
        DHCPGlutton* g = get_glutton();
        if (g && g->is_running()) {
            Serial.printf("State: RUNNING\n");
            Serial.printf("Reconnects: %d\n", g->get_reconnect_count());
            Serial.printf("Current IP: %s\n", g->get_current_ip().c_str());
            Serial.printf("Current MAC: %s\n", g->get_current_mac_str().c_str());
        } else {
            Serial.printf("State: Stopped\n");
            Serial.printf("ESP32 MAC: %s\n", wifi_manager_get_mac_str().c_str());
        }
    } else if (cmd == "save") {
        save_config();
    } else if (cmd == "load") {
        load_config();
    } else if (cmd == "reset") {
        reset_config();
    } else if (cmd == "help") {
        print_help();
    } else {
        Serial.println("Unknown. Type 'help'");
    }
}

void serial_cli_init() {
    Serial.begin(115200);
    delay(1000);
    load_config();
    print_help();
    Serial.print("\n> ");
}

void serial_cli_task() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (s_input.length() > 0) {
                Serial.println();
                handle_command(s_input);
                s_input = "";
                Serial.print("> ");
            }
        } else if (c == '\b' || c == 127) {
            if (s_input.length() > 0) {
                s_input.remove(s_input.length() - 1);
                Serial.print("\b \b");
            }
        } else {
            s_input += c;
            Serial.print(c);
        }
    }
}

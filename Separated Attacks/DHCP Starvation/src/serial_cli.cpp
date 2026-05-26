#include "serial_cli.h"
#include "wifi_manager.h"
#include "dhcp_starvation.h"
#include "config.h"

static String s_input;

static void print_help() {
    Serial.println("\n=== DHCP Starvation ===");
    Serial.println("Commands:");
    Serial.println("  set_ssid <name>          Set SSID");
    Serial.println("  set_pass <password>      Set password");
    Serial.println("  set_cooldown <ms>        Delay between reconnects (500-10000)");
    Serial.println("  connect                  Connect to WiFi");
    Serial.println("  start                    Begin DHCP starvation");
    Serial.println("  stop                     Stop + restore MAC");
    Serial.println("  status                   Show current stats");
    Serial.println("  save                     Save config to flash");
    Serial.println("  load                     Load config from flash");
    Serial.println("  reset                    Reset defaults");
    Serial.println("  help                     Show this help");
}

static void handle_command(const String& cmd) {
    if (cmd.startsWith("set_ssid ")) {
        strncpy(g_config.ssid, cmd.substring(9).c_str(), sizeof(g_config.ssid) - 1);
        Serial.printf("SSID: %s\n", g_config.ssid);
    } else if (cmd.startsWith("set_pass ")) {
        strncpy(g_config.password, cmd.substring(9).c_str(), sizeof(g_config.password) - 1);
        Serial.println("Password set");
    } else if (cmd.startsWith("set_cooldown ")) {
        g_config.cooldown_ms = cmd.substring(13).toInt();
        if (g_config.cooldown_ms < 500) g_config.cooldown_ms = 500;
        if (g_config.cooldown_ms > 10000) g_config.cooldown_ms = 10000;
        Serial.printf("Cooldown: %d ms\n", g_config.cooldown_ms);
    } else if (cmd == "connect") {
        if (strlen(g_config.ssid) == 0) {
            Serial.println("Error: SSID not set");
            return;
        }
        wifi_manager_connect(g_config.ssid, g_config.password);
    } else if (cmd == "start") {
        if (strlen(g_config.ssid) == 0) {
            Serial.println("Error: SSID not set");
            return;
        }
        if (!wifi_manager_is_connected()) {
            Serial.println("[DHCP] Auto-connecting...");
            if (!wifi_manager_connect(g_config.ssid, g_config.password)) {
                Serial.println("[DHCP] Connect failed");
                return;
            }
        }
        dhcp_start();
    } else if (cmd == "stop") {
        dhcp_stop();
    } else if (cmd == "status") {
        Serial.println("--- Status ---");
        Serial.printf("SSID: %s\n", g_config.ssid);
        Serial.printf("WiFi: %s\n", wifi_manager_is_connected() ? "UP" : "DOWN");
        Serial.printf("Cooldown: %d ms\n", g_config.cooldown_ms);
        if (dhcp_is_running()) {
            Serial.printf("State: RUNNING\n");
            Serial.printf("Reconnects: %d\n", dhcp_get_reconnect_count());
            Serial.printf("Current IP: %s\n", dhcp_get_current_ip().c_str());
            Serial.printf("Current MAC: %s\n", dhcp_get_current_mac().c_str());
        } else {
            Serial.printf("State: STOPPED\n");
            Serial.printf("ESP32 MAC: %s\n", dhcp_get_original_mac().c_str());
        }
    } else if (cmd == "save") {
        config_save();
        Serial.println("Saved");
    } else if (cmd == "load") {
        config_load();
        Serial.println("Loaded");
    } else if (cmd == "reset") {
        config_reset();
        Serial.println("Reset to defaults");
    } else if (cmd == "help") {
        print_help();
    } else {
        Serial.println("Unknown. Type 'help'");
    }
}

void serial_cli_init() {
    Serial.begin(115200);
    delay(1000);
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

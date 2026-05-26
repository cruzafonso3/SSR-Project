#include "serial_cli.h"
#include "wifi_manager.h"
#include "handshake_sniffer.h"
#include <Preferences.h>
#include <string.h>

static String s_input;
static Preferences prefs;

static char s_ssid[33] = "";
static int s_channel = 1;
static uint8_t s_bssid[6] = {0};
static bool s_sniffer_running = false;

static void print_help() {
    Serial.println("\n========================================");
    Serial.println("  WPA Handshake Capture - Educational");
    Serial.println("  FOR AUTHORIZED LAB USE ONLY");
    Serial.println("========================================\n");
    Serial.println("Commands:");
    Serial.println("  set_ssid <name>          Target AP SSID");
    Serial.println("  set_bssid <mac>          Target AP BSSID (AA:BB:CC:DD:EE:FF)");
    Serial.println("  set_channel <1-14>       WiFi channel to sniff");
    Serial.println("  start                    Start promiscuous sniffing");
    Serial.println("  stop                     Stop sniffing");
    Serial.println("  status                   Show capture status");
    Serial.println("  save                     Save capture to SPIFFS (/handshake.bin)");
    Serial.println("  load                     Load capture from SPIFFS");
    Serial.println("  dump                     Print handshake hex over serial");
    Serial.println("  dump_raw                 Dump raw handshake binary over serial");
    Serial.println("  reset                    Reset capture state");
    Serial.println("  help                     Show this help");
}

static bool parse_mac(const String& mac_str, uint8_t *out) {
    if (mac_str.length() != 17) return false;
    for (int i = 0; i < 6; i++) {
        String byte_str = mac_str.substring(i * 3, i * 3 + 2);
        out[i] = (uint8_t) strtol(byte_str.c_str(), NULL, 16);
    }
    return true;
}

static void print_mac_inline(const uint8_t *mac) {
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void save_config() {
    prefs.begin("wpa_cap", false);
    prefs.putString("ssid", s_ssid);
    prefs.putInt("channel", s_channel);
    prefs.putBytes("bssid", s_bssid, 6);
    prefs.end();
    Serial.println("Config saved");
}

static void load_config() {
    prefs.begin("wpa_cap", true);
    String s = prefs.getString("ssid", "");
    strncpy(s_ssid, s.c_str(), sizeof(s_ssid) - 1);
    s_channel = prefs.getInt("channel", 1);
    size_t len = prefs.getBytesLength("bssid");
    if (len == 6) prefs.getBytes("bssid", s_bssid, 6);
    prefs.end();

    if (strlen(s_ssid) > 0) sniffer_set_target_ssid(s_ssid);
    if (memcmp(s_bssid, "\x00\x00\x00\x00\x00\x00", 6) != 0) {
        sniffer_set_target_bssid(s_bssid);
    }
}

static void handle_command(const String& cmd) {
    if (cmd.startsWith("set_ssid ")) {
        strncpy(s_ssid, cmd.substring(9).c_str(), sizeof(s_ssid) - 1);
        Serial.printf("SSID: %s\n", s_ssid);
        sniffer_set_target_ssid(s_ssid);
    } else if (cmd.startsWith("set_bssid ")) {
        if (parse_mac(cmd.substring(10), s_bssid)) {
            Serial.print("BSSID: "); print_mac_inline(s_bssid); Serial.println();
            sniffer_set_target_bssid(s_bssid);
        } else {
            Serial.println("Error: Invalid MAC format. Use AA:BB:CC:DD:EE:FF");
        }
    } else if (cmd.startsWith("set_channel ")) {
        s_channel = cmd.substring(12).toInt();
        if (s_channel < 1) s_channel = 1;
        if (s_channel > 14) s_channel = 14;
        Serial.printf("Channel: %d\n", s_channel);
        if (s_sniffer_running) {
            wifi_manager_set_channel(s_channel);
        }
    } else if (cmd == "start") {
        if (strlen(s_ssid) == 0 && !sniffer_has_bssid_filter()) {
            Serial.println("Error: Set SSID or BSSID first");
            return;
        }
        wifi_manager_set_channel(s_channel);
        wifi_manager_promiscuous_start();
        s_sniffer_running = true;
        Serial.println("[Sniffer] STARTED");
        Serial.print("[Sniffer] Ch=");
        Serial.print(s_channel);
        Serial.print("  BSSID=");
        print_mac_inline(s_bssid);
        Serial.println();
    } else if (cmd == "stop") {
        wifi_manager_promiscuous_stop();
        s_sniffer_running = false;
        Serial.println("[Sniffer] STOPPED");
    } else if (cmd == "status") {
        Serial.println("--- Status ---");
        Serial.printf("SSID: %s\n", s_ssid);
        Serial.printf("Channel: %d\n", s_channel);
        Serial.printf("Sniffer: %s\n",
                      s_sniffer_running ? "RUNNING" : "STOPPED");
        Serial.printf("ESP32 MAC: %s\n", wifi_manager_get_mac_str().c_str());
        sniffer_print_status();
    } else if (cmd == "save") {
        sniffer_save_to_spiffs("/handshake.bin");
        save_config();
    } else if (cmd == "load") {
        sniffer_load_from_spiffs("/handshake.bin");
        load_config();
    } else if (cmd == "dump") {
        sniffer_dump_serial();
    } else if (cmd == "dump_raw") {
        sniffer_dump_raw();
    } else if (cmd == "reset") {
        sniffer_reset_capture();
        Serial.println("Capture state reset");
    } else if (cmd == "help") {
        print_help();
    } else {
        Serial.println("Unknown command. Type 'help'");
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

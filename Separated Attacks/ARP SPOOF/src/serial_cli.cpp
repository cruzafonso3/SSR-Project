#include "serial_cli.h"
#include "config.h"
#include "wifi_manager.h"
#include "arp_engine.h"
#include "packet_forwarder.h"
#include "dns_spoof.h"

static String s_input;

static void print_help() {
    Serial.println("Commands:");
    Serial.println("  set_ssid <name>          Set SSID");
    Serial.println("  set_pass <pass>          Set password");
    Serial.println("  set_target_ip <ip>       Set target IP");
    Serial.println("  set_target_mac <mac>     Set target MAC");
    Serial.println("  set_gateway_ip <ip>      Set gateway IP");
    Serial.println("  set_gateway_mac <mac>    Set gateway MAC");
    Serial.println("  set_dns_ip <ip>          Set DNS spoof IP");
    Serial.println("  set_dns_domain <dom>     Set DNS domain (* for all)");
    Serial.println("  set_arp_cooldown <ms>    Set ARP interval (100-10000ms)");
    Serial.println("  connect                  Connect WiFi");
    Serial.println("  disconnect               Disconnect WiFi");
    Serial.println("  start                    Start ARP + forward");
    Serial.println("  stop                     Stop all");
    Serial.println("  dns_start                Enable DNS spoof");
    Serial.println("  dns_stop                 Disable DNS spoof");
    Serial.println("  status                   Show status");
    Serial.println("  save                     Save config");
    Serial.println("  load                     Load config");
    Serial.println("  reset                    Reset defaults");
}

static void handle_command(const String& cmd) {
    if (cmd.startsWith("set_ssid ")) {
        strncpy(g_config.ssid, cmd.substring(9).c_str(), sizeof(g_config.ssid)-1);
        Serial.printf("SSID: %s\n", g_config.ssid);
    } else if (cmd.startsWith("set_pass ")) {
        strncpy(g_config.password, cmd.substring(9).c_str(), sizeof(g_config.password)-1);
        Serial.println("Password set");
    } else if (cmd.startsWith("set_target_ip ")) {
        strncpy(g_config.target_ip, cmd.substring(14).c_str(), sizeof(g_config.target_ip)-1);
        Serial.printf("Target IP: %s\n", g_config.target_ip);
    } else if (cmd.startsWith("set_target_mac ")) {
        strncpy(g_config.target_mac, cmd.substring(15).c_str(), sizeof(g_config.target_mac)-1);
        Serial.printf("Target MAC: %s\n", g_config.target_mac);
    } else if (cmd.startsWith("set_gateway_ip ")) {
        strncpy(g_config.gateway_ip, cmd.substring(15).c_str(), sizeof(g_config.gateway_ip)-1);
        Serial.printf("Gateway IP: %s\n", g_config.gateway_ip);
    } else if (cmd.startsWith("set_gateway_mac ")) {
        strncpy(g_config.gateway_mac, cmd.substring(16).c_str(), sizeof(g_config.gateway_mac)-1);
        Serial.printf("Gateway MAC: %s\n", g_config.gateway_mac);
    } else if (cmd.startsWith("set_dns_ip ")) {
        strncpy(g_config.dns_spoof_ip, cmd.substring(11).c_str(), sizeof(g_config.dns_spoof_ip)-1);
        dns_spoof_set_ip(g_config.dns_spoof_ip);
        Serial.printf("DNS IP: %s\n", g_config.dns_spoof_ip);
    } else if (cmd.startsWith("set_dns_domain ")) {
        strncpy(g_config.dns_spoof_domain, cmd.substring(15).c_str(), sizeof(g_config.dns_spoof_domain)-1);
        dns_spoof_set_domain(g_config.dns_spoof_domain);
        Serial.printf("DNS Domain: %s\n", g_config.dns_spoof_domain);
    } else if (cmd.startsWith("set_arp_cooldown ")) {
        g_config.arp_cooldown_ms = cmd.substring(17).toInt();
        if (g_config.arp_cooldown_ms < 100) g_config.arp_cooldown_ms = 100;
        if (g_config.arp_cooldown_ms > 10000) g_config.arp_cooldown_ms = 10000;
        Serial.printf("ARP cooldown: %d ms\n", g_config.arp_cooldown_ms);
    } else if (cmd == "connect") {
        if (strlen(g_config.ssid) == 0) {
            Serial.println("Error: SSID not set");
            return;
        }
        wifi_manager_connect(g_config.ssid, g_config.password);
    } else if (cmd == "disconnect") {
        wifi_manager_disconnect();
        arp_engine_stop();
        dns_spoof_stop();
        packet_forwarder_stop();
    } else if (cmd == "start") {
        if (!wifi_manager_is_connected()) {
            Serial.println("Error: Not connected");
            return;
        }
        arp_engine_start();
        packet_forwarder_start();
        Serial.println("Started. Use 'dns_start' for DNS spoof");
    } else if (cmd == "dns_start") {
        dns_spoof_start();
        Serial.println("[DNS] ON");
    } else if (cmd == "dns_stop") {
        dns_spoof_stop();
        Serial.println("[DNS] OFF");
    } else if (cmd == "stop") {
        arp_engine_stop();
        dns_spoof_stop();
        packet_forwarder_stop();
    } else if (cmd == "status") {
        Serial.printf("WiFi: %s  ARP: %s  Fwd: %s  DNS: %s\n",
            wifi_manager_is_connected() ? "UP" : "DOWN",
            arp_engine_is_running() ? "ON" : "OFF",
            packet_forwarder_is_running() ? "ON" : "OFF",
            dns_spoof_is_running() ? "ON" : "OFF");
        Serial.printf("ARP Cooldown: %d ms\n", g_config.arp_cooldown_ms);
        Serial.printf("Target: %s  GW: %s\n", g_config.target_ip, g_config.gateway_ip);
        uint32_t cap, fwd, drop, dns;
        packet_forwarder_get_stats(cap, fwd, drop, dns);
        Serial.printf("Stats: Cap=%u Fwd=%u Drop=%u DNS=%u\n", cap, fwd, drop, dns);
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
        Serial.println("Unknown command. Type 'help'");
    }
}

void serial_cli_init() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nARP Spoof - Educational Demo");
    print_help();
    Serial.print("> ");
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

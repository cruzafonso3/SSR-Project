#ifndef HANDSHAKE_SNIFFER_H
#define HANDSHAKE_SNIFFER_H

#include <Arduino.h>
#include <esp_wifi.h>

void sniffer_init();

void sniffer_set_target_bssid(const uint8_t *bssid);
void sniffer_set_target_ssid(const char *ssid);

bool sniffer_has_bssid_filter();

void sniffer_reset_capture();
bool sniffer_has_complete_handshake();
void sniffer_print_status();
void sniffer_dump_serial();
void sniffer_dump_raw();

bool sniffer_save_to_spiffs(const char *filename);
bool sniffer_load_from_spiffs(const char *filename);

#endif

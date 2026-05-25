#include "handshake_sniffer.h"
#include <FS.h>
#include <SPIFFS.h>
#include <string.h>

#define SNAP_HEADER_LEN     8
#define EAPOL_ETHERTYPE     0x888E
#define EAPOL_KEY_TYPE      3

static bool s_has_bssid_filter = false;
static uint8_t s_target_bssid[6] = {0};

static char s_target_ssid[33] = {0};

struct EapolFrame {
    uint8_t data[512];
    uint16_t len;
};

static uint8_t s_anonce[32] = {0};
static uint8_t s_snonce[32] = {0};
static uint8_t s_mic_m2[16] = {0};
static uint8_t s_mic_m3[16] = {0};

static EapolFrame s_m1 = {{0}, 0};
static EapolFrame s_m2 = {{0}, 0};
static EapolFrame s_m3 = {{0}, 0};
static EapolFrame s_m4 = {{0}, 0};

static bool s_got_m1 = false;
static bool s_got_m2 = false;
static bool s_got_m3 = false;
static bool s_got_m4 = false;

static uint8_t s_captured_bssid[6] = {0};
static uint8_t s_captured_stamac[6] = {0};

static uint32_t s_eapol_seen = 0;

static void promiscuous_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type);

void sniffer_init() {
    if (!SPIFFS.begin(true)) {
        Serial.println("[Sniffer] SPIFFS mount failed");
    }
    esp_wifi_set_promiscuous_rx_cb(promiscuous_rx_cb);
    sniffer_reset_capture();
}

static bool mac_equal(const uint8_t *a, const uint8_t *b) {
    return memcmp(a, b, 6) == 0;
}

static void print_mac_inline(const uint8_t *mac) {
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static uint8_t detect_eapol_key_msg(const uint8_t *eapol_body, uint16_t eapol_body_len,
                                     uint8_t *out_nonce, uint8_t *out_mic) {
    if (eapol_body_len < 3) return 0;

    uint8_t desc_type = eapol_body[0];
    if (desc_type != 2) return 0; // EAPOL RSN Key (2) only

    // Key Information is BIG-ENDIAN: byte[1] is high, byte[2] is low
    uint16_t key_info = (eapol_body[1] << 8) | eapol_body[2];

    bool key_ack  = (key_info >> 7) & 0x1;
    bool key_mic  = (key_info >> 8) & 0x1;
    bool secure   = (key_info >> 9) & 0x1;

    uint8_t msg_type = 0;
    if (key_ack && !key_mic && !secure)      msg_type = 1; // M1
    else if (!key_ack && key_mic && !secure) msg_type = 2; // M2
    else if (key_ack && key_mic && secure)   msg_type = 3; // M3
    else if (!key_ack && key_mic && secure)  msg_type = 4; // M4

    if (msg_type == 0) return 0;

    // Nonce at offset 13 (desc_type[1] + key_info[2] + key_length[2] + replay_counter[8])
    if (eapol_body_len >= 13 + 32) {
        memcpy(out_nonce, eapol_body + 13, 32);
    }

    // MIC at offset 77 (after nonce[32] + iv[16] + rsc[8] + keyid[8])
    if (eapol_body_len >= 77 + 16) {
        memcpy(out_mic, eapol_body + 77, 16);
    }

    return msg_type;
}

static void promiscuous_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!buf) return;

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint8_t* payload = pkt->payload;
    uint16_t len = pkt->rx_ctrl.sig_len;

    if (len < 36) return;

    uint8_t frame_type = payload[0];

    // Only process Data (0x08) or QoS Data (0x88) frames
    if (frame_type != 0x08 && frame_type != 0x88) return;

    // Determine direction from FC flags (byte 1)
    uint8_t to_ds   = (payload[1] >> 0) & 0x1;
    uint8_t from_ds = (payload[1] >> 1) & 0x1;

    uint8_t bssid[6];
    uint8_t stamac[6];

    if (to_ds && !from_ds) {
        // STA -> AP: Addr1 = BSSID, Addr2 = STA
        memcpy(bssid, payload + 4, 6);
        memcpy(stamac, payload + 10, 6);
    } else if (!to_ds && from_ds) {
        // AP -> STA: Addr1 = STA, Addr2 = BSSID
        memcpy(stamac, payload + 4, 6);
        memcpy(bssid, payload + 10, 6);
    } else {
        return; // WDS or ad-hoc
    }

    if (s_has_bssid_filter && !mac_equal(bssid, s_target_bssid)) return;

    // Check EAPOL ethertype at the correct LLC/SNAP offset
    // Data (24-byte hdr): EtherType at offset 30
    // QoS Data (26-byte hdr): EtherType at offset 32
    int hdr_len = (frame_type == 0x88) ? 26 : 24;
    if (len < (uint16_t)(hdr_len + SNAP_HEADER_LEN + 4)) return;

    // LLC/SNAP: AA AA 03 00 00 00 XX XX
    if (payload[hdr_len + 0] != 0xAA || payload[hdr_len + 1] != 0xAA || payload[hdr_len + 2] != 0x03) return;
    if (payload[hdr_len + 3] != 0x00 || payload[hdr_len + 4] != 0x00 || payload[hdr_len + 5] != 0x00) return;

    // EtherType is BIG-ENDIAN: high byte first
    uint16_t eth_type = (payload[hdr_len + 6] << 8) | payload[hdr_len + 7];
    if (eth_type != EAPOL_ETHERTYPE) return;

    s_eapol_seen++;

    uint8_t* eapol = payload + hdr_len + SNAP_HEADER_LEN;
    uint16_t eapol_pkt_len = len - hdr_len - SNAP_HEADER_LEN;
    if (eapol_pkt_len >= 4) eapol_pkt_len -= 4;

    if (eapol_pkt_len < 4) return;
    uint8_t eapol_type = eapol[1];
    if (eapol_type != EAPOL_KEY_TYPE) return;

    uint16_t eapol_body_len = (eapol[2] << 8) | eapol[3];
    if (eapol_body_len > eapol_pkt_len - 4) eapol_body_len = eapol_pkt_len - 4;
    if (eapol_body_len < 3) return;

    uint8_t nonce[32] = {0};
    uint8_t mic[16] = {0};
    uint8_t msg_type = detect_eapol_key_msg(eapol + 4, eapol_body_len, nonce, mic);
    if (msg_type == 0) return;

    memcpy(s_captured_bssid, bssid, 6);
    memcpy(s_captured_stamac, stamac, 6);

    bool new_msg = false;
    switch (msg_type) {
        case 1:
            if (!s_got_m1) {
                s_got_m1 = true;
                memcpy(s_anonce, nonce, 32);
                if (eapol_pkt_len <= sizeof(s_m1.data)) {
                    memcpy(s_m1.data, eapol, eapol_pkt_len);
                    s_m1.len = eapol_pkt_len;
                }
                new_msg = true;
                Serial.print("[HS] M1/4 captured  BSSID: ");
                print_mac_inline(bssid);
                Serial.print("  STA: ");
                print_mac_inline(stamac);
                Serial.println();
            }
            break;
        case 2:
            if (!s_got_m2) {
                s_got_m2 = true;
                memcpy(s_snonce, nonce, 32);
                memcpy(s_mic_m2, mic, 16);
                if (eapol_pkt_len <= sizeof(s_m2.data)) {
                    memcpy(s_m2.data, eapol, eapol_pkt_len);
                    s_m2.len = eapol_pkt_len;
                }
                new_msg = true;
                Serial.print("[HS] M2/4 captured  BSSID: ");
                print_mac_inline(bssid);
                Serial.print("  STA: ");
                print_mac_inline(stamac);
                Serial.println();
            }
            break;
        case 3:
            if (!s_got_m3) {
                s_got_m3 = true;
                memcpy(s_mic_m3, mic, 16);
                if (eapol_pkt_len <= sizeof(s_m3.data)) {
                    memcpy(s_m3.data, eapol, eapol_pkt_len);
                    s_m3.len = eapol_pkt_len;
                }
                new_msg = true;
                Serial.print("[HS] M3/4 captured  BSSID: ");
                print_mac_inline(bssid);
                Serial.print("  STA: ");
                print_mac_inline(stamac);
                Serial.println();
            }
            break;
        case 4:
            if (!s_got_m4) {
                s_got_m4 = true;
                if (eapol_pkt_len <= sizeof(s_m4.data)) {
                    memcpy(s_m4.data, eapol, eapol_pkt_len);
                    s_m4.len = eapol_pkt_len;
                }
                new_msg = true;
                Serial.print("[HS] M4/4 captured  BSSID: ");
                print_mac_inline(bssid);
                Serial.print("  STA: ");
                print_mac_inline(stamac);
                Serial.println();
            }
            break;
    }

    if (new_msg && sniffer_has_complete_handshake()) {
        Serial.println("[HS] *** COMPLETE HANDSHAKE (M1+M2) ***");
    }
}

void sniffer_set_target_bssid(const uint8_t *bssid) {
    memcpy(s_target_bssid, bssid, 6);
    s_has_bssid_filter = true;
}

void sniffer_set_target_ssid(const char *ssid) {
    strncpy(s_target_ssid, ssid, sizeof(s_target_ssid) - 1);
    s_target_ssid[sizeof(s_target_ssid) - 1] = '\0';
}

bool sniffer_has_bssid_filter() { return s_has_bssid_filter; }

void sniffer_reset_capture() {
    memset(s_anonce, 0, 32);
    memset(s_snonce, 0, 32);
    memset(s_mic_m2, 0, 16);
    memset(s_mic_m3, 0, 16);
    memset(&s_m1, 0, sizeof(s_m1));
    memset(&s_m2, 0, sizeof(s_m2));
    memset(&s_m3, 0, sizeof(s_m3));
    memset(&s_m4, 0, sizeof(s_m4));
    s_got_m1 = false;
    s_got_m2 = false;
    s_got_m3 = false;
    s_got_m4 = false;
    s_eapol_seen = 0;
    memset(s_captured_bssid, 0, 6);
    memset(s_captured_stamac, 0, 6);
}

bool sniffer_has_complete_handshake() {
    return s_got_m1 && s_got_m2;
}

void sniffer_print_status() {
    Serial.printf("M1: %s  M2: %s  M3: %s  M4: %s\n",
                  s_got_m1 ? "YES" : "no",
                  s_got_m2 ? "YES" : "no",
                  s_got_m3 ? "YES" : "no",
                  s_got_m4 ? "YES" : "no");
    Serial.printf("EAPOL frames seen: %lu\n", (unsigned long)s_eapol_seen);
    if (s_got_m1 || s_got_m2) {
        Serial.print("BSSID: "); print_mac_inline(s_captured_bssid); Serial.println();
        Serial.print("STA:   "); print_mac_inline(s_captured_stamac); Serial.println();
    }
}

static void print_hex_block(const uint8_t *data, uint16_t len, const char *label) {
    Serial.printf("%s (%d bytes):\n", label, len);
    for (int i = 0; i < len; i++) {
        Serial.printf("%02X", data[i]);
        if ((i + 1) % 32 == 0) Serial.println();
    }
    if (len % 32 != 0) Serial.println();
}

void sniffer_dump_serial() {
    if (!sniffer_has_complete_handshake()) {
        Serial.println("No complete handshake to dump");
        return;
    }
    Serial.println("========== HANDSHAKE DUMP ==========");
    Serial.print("BSSID: "); print_mac_inline(s_captured_bssid); Serial.println();
    Serial.print("STA:   "); print_mac_inline(s_captured_stamac); Serial.println();
    Serial.printf("SSID:  %s\n", s_target_ssid);
    print_hex_block(s_anonce, 32, "ANonce");
    print_hex_block(s_snonce, 32, "SNonce");
    print_hex_block(s_mic_m2, 16, "MIC_M2");
    if (s_m1.len) print_hex_block(s_m1.data, s_m1.len, "M1");
    if (s_m2.len) print_hex_block(s_m2.data, s_m2.len, "M2");
    if (s_m3.len) print_hex_block(s_m3.data, s_m3.len, "M3");
    if (s_m4.len) print_hex_block(s_m4.data, s_m4.len, "M4");
    Serial.println("====================================");
}

static void print_hex_line(const uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
    }
    Serial.println();
}

void sniffer_dump_raw() {
    if (!sniffer_has_complete_handshake()) {
        Serial.println("[HS] Error: No complete handshake to dump");
        return;
    }

    Serial.println("---HEX START---");

    print_hex_line((const uint8_t*)"WPA1", 4);
    print_hex_line(s_captured_bssid, 6);
    print_hex_line(s_captured_stamac, 6);

    uint8_t ssid_len = (uint8_t)strlen(s_target_ssid);
    Serial.printf("%02X\n", ssid_len);

    char ssid_buf[32] = {0};
    memcpy(ssid_buf, s_target_ssid, ssid_len);
    print_hex_line((uint8_t*)ssid_buf, 32);

    print_hex_line(s_anonce, 32);
    print_hex_line(s_snonce, 32);
    print_hex_line(s_mic_m2, 16);
    print_hex_line(s_mic_m3, 16);

    print_hex_line((uint8_t*)&s_m1.len, 2);
    print_hex_line((uint8_t*)&s_m2.len, 2);
    print_hex_line((uint8_t*)&s_m3.len, 2);
    print_hex_line((uint8_t*)&s_m4.len, 2);

    if (s_m1.len > 0) print_hex_line(s_m1.data, s_m1.len);
    if (s_m2.len > 0) print_hex_line(s_m2.data, s_m2.len);
    if (s_m3.len > 0) print_hex_line(s_m3.data, s_m3.len);
    if (s_m4.len > 0) print_hex_line(s_m4.data, s_m4.len);

    Serial.println("---HEX END---");
    Serial.println("[HS] Hex dump complete");
}

bool sniffer_save_to_spiffs(const char *filename) {
    if (!sniffer_has_complete_handshake()) {
        Serial.println("[HS] Error: No complete handshake to save");
        return false;
    }

    File f = SPIFFS.open(filename, FILE_WRITE);
    if (!f) {
        Serial.println("[HS] Error: Cannot open file for writing");
        return false;
    }

    char magic[4] = {'W', 'P', 'A', '1'};
    f.write((uint8_t*)magic, 4);
    f.write(s_captured_bssid, 6);
    f.write(s_captured_stamac, 6);

    uint8_t ssid_len = (uint8_t)strlen(s_target_ssid);
    f.write(&ssid_len, 1);

    char ssid_buf[32] = {0};
    memcpy(ssid_buf, s_target_ssid, ssid_len);
    f.write((uint8_t*)ssid_buf, 32);

    f.write(s_anonce, 32);
    f.write(s_snonce, 32);
    f.write(s_mic_m2, 16);
    f.write(s_mic_m3, 16);

    f.write((uint8_t*)&s_m1.len, 2);
    f.write((uint8_t*)&s_m2.len, 2);
    f.write((uint8_t*)&s_m3.len, 2);
    f.write((uint8_t*)&s_m4.len, 2);

    if (s_m1.len > 0) f.write(s_m1.data, s_m1.len);
    if (s_m2.len > 0) f.write(s_m2.data, s_m2.len);
    if (s_m3.len > 0) f.write(s_m3.data, s_m3.len);
    if (s_m4.len > 0) f.write(s_m4.data, s_m4.len);

    f.close();
    Serial.printf("[HS] Saved to %s\n", filename);
    return true;
}

bool sniffer_load_from_spiffs(const char *filename) {
    if (!SPIFFS.exists(filename)) return false;

    File f = SPIFFS.open(filename, FILE_READ);
    if (!f) return false;

    char magic[4];
    f.read((uint8_t*)magic, 4);
    if (magic[0] != 'W' || magic[1] != 'P' || magic[2] != 'A' || magic[3] != '1') {
        f.close();
        return false;
    }

    f.read(s_captured_bssid, 6);
    f.read(s_captured_stamac, 6);

    uint8_t ssid_len;
    f.read(&ssid_len, 1);

    char ssid_buf[32] = {0};
    f.read((uint8_t*)ssid_buf, 32);
    memcpy(s_target_ssid, ssid_buf, ssid_len);
    s_target_ssid[ssid_len] = '\0';

    f.read(s_anonce, 32);
    f.read(s_snonce, 32);
    f.read(s_mic_m2, 16);
    f.read(s_mic_m3, 16);

    f.read((uint8_t*)&s_m1.len, 2);
    f.read((uint8_t*)&s_m2.len, 2);
    f.read((uint8_t*)&s_m3.len, 2);
    f.read((uint8_t*)&s_m4.len, 2);

    if (s_m1.len > 0 && s_m1.len <= sizeof(s_m1.data)) { f.read(s_m1.data, s_m1.len); s_got_m1 = true; }
    if (s_m2.len > 0 && s_m2.len <= sizeof(s_m2.data)) { f.read(s_m2.data, s_m2.len); s_got_m2 = true; }
    if (s_m3.len > 0 && s_m3.len <= sizeof(s_m3.data)) { f.read(s_m3.data, s_m3.len); s_got_m3 = true; }
    if (s_m4.len > 0 && s_m4.len <= sizeof(s_m4.data)) { f.read(s_m4.data, s_m4.len); s_got_m4 = true; }

    f.close();
    Serial.printf("[HS] Loaded from %s\n", filename);
    return true;
}

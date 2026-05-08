#include "packet_utils.h"
#include <esp_wifi.h>

const uint8_t LLC_SNAP_IP[8]  = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00};
const uint8_t LLC_SNAP_ARP[8] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x08, 0x06};

bool inject80211(const uint8_t* addr3_dest, const uint8_t* addr2_src,
                 const uint8_t* addr1_bssid, const uint8_t* llc_snap,
                 const uint8_t* payload, uint16_t payload_len) {
    uint8_t buffer[512];
    uint16_t totalLen = DOT11_DATA_HDR_LEN + LLC_SNAP_LEN + payload_len;
    if (totalLen > sizeof(buffer)) return false;
    buffer[0] = 0x02; buffer[1] = 0x01;
    buffer[2] = 0x00; buffer[3] = 0x00;
    memcpy(&buffer[4], addr1_bssid, 6);
    memcpy(&buffer[10], addr2_src, 6);
    memcpy(&buffer[16], addr3_dest, 6);
    buffer[22] = 0x00; buffer[23] = 0x00;
    memcpy(&buffer[24], llc_snap, 8);
    memcpy(&buffer[32], payload, payload_len);
    return esp_wifi_80211_tx(WIFI_IF_STA, buffer, totalLen, true) == ESP_OK;
}

uint16_t ipChecksum(const uint8_t* data, uint16_t len) {
    uint32_t sum = 0;
    for (uint16_t i = 0; i < len; i += 2) {
        uint16_t word = data[i];
        if (i + 1 < len) word |= (data[i + 1] << 8);
        sum += word;
    }
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

void printMAC(const uint8_t* mac) {
    for (int i = 0; i < 6; i++) {
        if (i > 0) Serial.print(":");
        if (mac[i] < 0x10) Serial.print("0");
        Serial.print(mac[i], HEX);
    }
    Serial.println();
}

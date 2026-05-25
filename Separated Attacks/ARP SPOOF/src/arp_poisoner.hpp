/**
 * ARP Poisoner - CapibaraZero implementation (adapted)
 * Uses esp_wifi_internal_tx with raw Ethernet frames.
 */

#ifndef ARP_POISONER_HPP
#define ARP_POISONER_HPP

#include <stdint.h>

typedef struct arp_hdr {
    uint16_t hw_type;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t opcode;
    uint8_t sender_mac[6];
    uint8_t sender_ip[4];
    uint8_t target_mac[6];
    uint8_t target_ip[4];
} arp_hdr;

#define ETH_HDRLEN 14
#define ARP_HDRLEN 28
#define ETH_HW_TYPE 0x0800
#define ARP_REPLY_OPCODE 2
#define MAC_ADDRESS_LENGTH 6
#define IPV4_LENGTH 4
#define ETH_ARP_HW_TYPE 1
#define ETHERNET_PROTOCOL_ARP 0x0806
#define PACKET_LENGTH 42

class ARP_poisoner {
private:
    uint8_t broadcast_mac_address[6];
    arp_hdr arp_pkt;
    uint8_t ether_frame[PACKET_LENGTH];
    uint8_t *get_current_ip(void);
    void fill_arp_hdr(uint8_t *src_mac, uint8_t *src_ip,
                      uint8_t *dest_mac, uint8_t *dest_ip);
    uint8_t src_mac[MAC_ADDRESS_LENGTH];
    uint8_t src_ip[IPV4_LENGTH];

public:
    // If custom_src_ip is provided, it is copied and used as sender IP.
    // Otherwise sender IP defaults to WiFi.gatewayIP().
    ARP_poisoner(uint8_t *custom_src_ip = nullptr);
    ~ARP_poisoner() = default;
    void send_arp_packet(uint8_t dest_ip[IPV4_LENGTH],
                         uint8_t dest_mac[MAC_ADDRESS_LENGTH]);
};

#endif

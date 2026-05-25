#ifndef DNS_SPOOF_H
#define DNS_SPOOF_H

#include <stdint.h>
#include <lwip/def.h>

#define DNS_SPOOF_MAX_RESPONSE 512

void dns_spoof_init();
void dns_spoof_start();
void dns_spoof_stop();
bool dns_spoof_is_running();
void dns_spoof_set_ip(const char* ip);
void dns_spoof_set_domain(const char* domain);
bool dns_spoof_process(uint8_t* ip_pkt, int ip_len, uint8_t* out, int* out_len);

#endif

#ifndef PACKET_FORWARDER_H
#define PACKET_FORWARDER_H

#include <stdint.h>

void packet_forwarder_init();
void packet_forwarder_start();
void packet_forwarder_stop();
bool packet_forwarder_is_running();
unsigned long packet_forwarder_last_traffic();
void packet_forwarder_get_stats(uint32_t& captured, uint32_t& forwarded, uint32_t& dropped, uint32_t& dns_spoofed);
void packet_forwarder_print_debug();

#endif

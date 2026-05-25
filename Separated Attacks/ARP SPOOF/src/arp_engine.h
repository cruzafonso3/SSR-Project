#ifndef ARP_ENGINE_H
#define ARP_ENGINE_H

#include <Arduino.h>

void arp_engine_init();
void arp_engine_start();
void arp_engine_stop();
bool arp_engine_is_running();
void arp_engine_task(); // call periodically from loop or task

#endif

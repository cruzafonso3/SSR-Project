#ifndef NETWORK_ATTACKS_H
#define NETWORK_ATTACKS_H

#include "config.h"

// Globals used by attack modules
extern uint8_t g_esp32_mac[6];
extern IPAddress g_esp32_ip;
extern uint8_t g_bssid[6];
extern uint8_t VICTIM_MAC[6];
extern IPAddress VICTIM_IP;
extern uint8_t GATEWAY_MAC[6];
extern IPAddress GATEWAY_IP;
extern char TARGET_DOMAIN_STR[64];
extern IPAddress SPOOFED_IP;

void attacksInit();
void attacksUpdate();
void attacksStart(int attackId);
void attacksStop(int attackId);
void attacksStopAll();
bool attacksIsRunning(int attackId);
int attacksGetCount(int attackId);
void promiscuousStart();
void promiscuousStop();

#endif
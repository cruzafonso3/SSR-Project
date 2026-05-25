#ifndef DHCP_GLUTTON_H
#define DHCP_GLUTTON_H

#include <Arduino.h>

class DHCPGlutton {
private:
    uint8_t original_mac[6];
    uint8_t random_mac[6];
    char ssid[32];
    char password[64];
    int cooldown_ms;
    bool running;
    unsigned long last_reconnect;
    int reconnect_count;
    String current_ip;

    void generate_random_mac();

public:
    DHCPGlutton(const char* ssid, const char* password, int cooldown_ms);
    ~DHCPGlutton();
    
    void start();
    void stop();
    bool is_running();
    void step();
    
    int get_reconnect_count() { return reconnect_count; }
    String get_current_ip() { return current_ip; }
    String get_current_mac_str();
    int get_cooldown() { return cooldown_ms; }
};

extern DHCPGlutton* get_glutton();
extern void create_glutton(const char* ssid, const char* pass, int cooldown);
extern void destroy_glutton();

#endif

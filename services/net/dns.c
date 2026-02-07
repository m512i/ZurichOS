/* DNS Resolver
 * Domain Name System
 */

#include <net/dns.h>
#include <net/net.h>
#include <net/udp.h>
#include <drivers/serial.h>
#include <string.h>

static uint32_t dns_server = 0;
static dns_cache_entry_t dns_cache[DNS_CACHE_SIZE];
static uint16_t dns_id = 1;
static volatile uint32_t dns_result_ip = 0;
static volatile int dns_result_ready = 0;

void dns_init(void)
{
    memset(dns_cache, 0, sizeof(dns_cache));
    dns_server = (8 << 24) | (8 << 16) | (8 << 8) | 8;  
    serial_puts("[DNS] Initialized\n");
}

void dns_set_server(uint32_t server_ip)
{
    dns_server = server_ip;
}

uint32_t dns_get_server(void)
{
    return dns_server;
}

int dns_cache_lookup(const char *hostname, uint32_t *ip)
{
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (dns_cache[i].valid && strcmp(dns_cache[i].name, hostname) == 0) {
            *ip = dns_cache[i].ip;
            return 0;
        }
    }
    return -1;
}

void dns_cache_add(const char *hostname, uint32_t ip, uint32_t ttl)
{
    int slot = 0;
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (!dns_cache[i].valid) {
            slot = i;
            break;
        }
    }
    
    strncpy(dns_cache[slot].name, hostname, DNS_MAX_NAME_LEN - 1);
    dns_cache[slot].name[DNS_MAX_NAME_LEN - 1] = '\0';
    dns_cache[slot].ip = ip;
    dns_cache[slot].ttl = ttl;
    dns_cache[slot].valid = 1;
}

static int dns_encode_name(uint8_t *buf, const char *name)
{
    int pos = 0;
    const char *p = name;
    
    while (*p) {
        const char *dot = p;
        while (*dot && *dot != '.') dot++;
        
        int len = dot - p;
        buf[pos++] = len;
        memcpy(&buf[pos], p, len);
        pos += len;
        
        p = *dot ? dot + 1 : dot;
    }
    buf[pos++] = 0;
    return pos;
}

int dns_resolve(const char *hostname, uint32_t *ip)
{
    if (!hostname || !ip) return -1;
    
    if (dns_cache_lookup(hostname, ip) == 0) {
        return 0;
    }
    
    if (dns_server == 0) return -1;
    
    uint8_t packet[512];
    memset(packet, 0, sizeof(packet));
    
    dns_header_t *hdr = (dns_header_t *)packet;
    uint16_t id = dns_id++;
    hdr->id = htons(id);
    hdr->flags = htons(DNS_FLAG_RD);  
    hdr->qdcount = htons(1);
    
    int name_len = dns_encode_name(packet + sizeof(dns_header_t), hostname);
    
    uint8_t *qtype = packet + sizeof(dns_header_t) + name_len;
    qtype[0] = 0; qtype[1] = DNS_TYPE_A;    
    qtype[2] = 0; qtype[3] = DNS_CLASS_IN; 
    
    int total_len = sizeof(dns_header_t) + name_len + 4;
    
    dns_result_ready = 0;
    dns_result_ip = 0;
    
    udp_send(dns_server, 53535, DNS_PORT, packet, total_len);
    serial_puts("[DNS] Sent query for: ");
    serial_puts(hostname);
    serial_puts("\n");
    
    for (int i = 0; i < 2000000 && !dns_result_ready; i++) {
        extern void net_poll(void);
        net_poll();
    }
    
    if (dns_result_ready && dns_result_ip != 0) {
        *ip = dns_result_ip;
        dns_cache_add(hostname, dns_result_ip, 300);
        return 0;
    }
    
    return -1;
}

void dns_receive(const void *data, uint16_t len)
{
    if (len < sizeof(dns_header_t)) return;
    
    const dns_header_t *hdr = (const dns_header_t *)data;
    uint16_t flags = ntohs(hdr->flags);
    
    if (!(flags & DNS_FLAG_QR)) return;
    
    if (flags & DNS_FLAG_RCODE) {
        serial_puts("[DNS] Query failed\n");
        dns_result_ready = 1;
        return;
    }
    
    uint16_t ancount = ntohs(hdr->ancount);
    if (ancount == 0) {
        serial_puts("[DNS] No answers\n");
        dns_result_ready = 1;
        return;
    }
    
    const uint8_t *ptr = (const uint8_t *)data + sizeof(dns_header_t);
    const uint8_t *end = (const uint8_t *)data + len;
    
    while (ptr < end && *ptr != 0) {
        if ((*ptr & 0xC0) == 0xC0) {
            ptr += 2;
            break;
        }
        ptr += *ptr + 1;
    }
    if (*ptr == 0) ptr++;
    ptr += 4;  
    
    for (int i = 0; i < ancount && ptr < end; i++) {
        if ((*ptr & 0xC0) == 0xC0) {
            ptr += 2;
        } else {
            while (ptr < end && *ptr != 0) ptr += *ptr + 1;
            if (ptr < end) ptr++;
        }
        
        if (ptr + 10 > end) break;
        
        uint16_t type = (ptr[0] << 8) | ptr[1];
        uint16_t rdlen = (ptr[8] << 8) | ptr[9];
        ptr += 10;
        
        if (type == DNS_TYPE_A && rdlen == 4 && ptr + 4 <= end) {
            dns_result_ip = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
            serial_puts("[DNS] Resolved to IP\n");
            dns_result_ready = 1;
            return;
        }
        
        ptr += rdlen;
    }
    
    dns_result_ready = 1;
}

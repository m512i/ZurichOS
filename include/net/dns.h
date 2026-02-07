/* DNS Resolver Header
 * Domain Name System
 */

#ifndef _NET_DNS_H
#define _NET_DNS_H

#include <stdint.h>

#define DNS_PORT            53
#define DNS_MAX_NAME_LEN    256
#define DNS_CACHE_SIZE      16

#define DNS_FLAG_QR         0x8000  
#define DNS_FLAG_OPCODE     0x7800  
#define DNS_FLAG_AA         0x0400  
#define DNS_FLAG_TC         0x0200  
#define DNS_FLAG_RD         0x0100  
#define DNS_FLAG_RA         0x0080  
#define DNS_FLAG_RCODE      0x000F  

#define DNS_TYPE_A          1   
#define DNS_TYPE_NS         2   
#define DNS_TYPE_CNAME      5   
#define DNS_TYPE_MX         15  
#define DNS_TYPE_AAAA       28  

#define DNS_CLASS_IN        1  

typedef struct dns_header {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed)) dns_header_t;

typedef struct dns_cache_entry {
    char name[DNS_MAX_NAME_LEN];
    uint32_t ip;
    uint32_t ttl;
    uint32_t timestamp;
    uint8_t valid;
} dns_cache_entry_t;

void dns_init(void);
void dns_set_server(uint32_t server_ip);
uint32_t dns_get_server(void);
int dns_resolve(const char *hostname, uint32_t *ip);
void dns_receive(const void *data, uint16_t len);
int dns_cache_lookup(const char *hostname, uint32_t *ip);
void dns_cache_add(const char *hostname, uint32_t ip, uint32_t ttl);

#endif

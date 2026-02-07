/* IPv4 Header
 * IP protocol definitions
 */

#ifndef _NET_IP_H
#define _NET_IP_H

#include <stdint.h>

#define IP_VERSION      4
#define IP_HEADER_LEN   20
#define IP_TTL_DEFAULT  64

#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6
#define IP_PROTO_UDP    17

typedef struct ip_header {
    uint8_t ihl_version;        
    uint8_t tos;                
    uint16_t total_len;         
    uint16_t id;                
    uint16_t frag_off;          
    uint8_t ttl;                
    uint8_t protocol;           
    uint16_t checksum;          
    uint32_t src_ip;            
    uint32_t dest_ip;           
} __attribute__((packed)) ip_header_t;

#define IP_ADDR(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
                             ((uint32_t)(c) << 8) | (uint32_t)(d))

void ip_send(uint32_t dest_ip, uint8_t protocol, const void *data, uint16_t len);
void ip_receive(const void *packet, uint16_t len);
uint16_t ip_checksum(const void *data, uint16_t len);

#endif

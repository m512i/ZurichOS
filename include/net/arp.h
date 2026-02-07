/* ARP Header
 * Address Resolution Protocol definitions
 */

#ifndef _NET_ARP_H
#define _NET_ARP_H

#include <stdint.h>

#define ARP_HTYPE_ETH   1       
#define ARP_PTYPE_IP    0x0800 

#define ARP_OP_REQUEST  1
#define ARP_OP_REPLY    2

typedef struct arp_header {
    uint16_t htype;             
    uint16_t ptype;             
    uint8_t hlen;               
    uint8_t plen;               
    uint16_t oper;              
    uint8_t sha[6];             
    uint32_t spa;               
    uint8_t tha[6];             
    uint32_t tpa;               
} __attribute__((packed)) arp_header_t;

typedef struct arp_entry {
    uint32_t ip;
    uint8_t mac[6];
    uint8_t valid;
    uint32_t timestamp;
} arp_entry_t;

void arp_init(void);
void arp_request(uint32_t ip);
void arp_receive(const void *packet, uint16_t len);
int arp_lookup(uint32_t ip, uint8_t *mac);
void arp_add_entry(uint32_t ip, const uint8_t *mac);

#endif

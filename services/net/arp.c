/* ARP
 * Address Resolution Protocol
 */

#include <net/arp.h>
#include <net/ethernet.h>
#include <net/net.h>
#include <drivers/serial.h>
#include <string.h>

#define ARP_CACHE_SIZE 32

static arp_entry_t arp_cache[ARP_CACHE_SIZE];
static const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void arp_init(void)
{
    memset(arp_cache, 0, sizeof(arp_cache));
    serial_puts("[ARP] Initialized\n");
}

void arp_add_entry(uint32_t ip, const uint8_t *mac)
{
    int slot = -1;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            slot = i;
            break;
        }
        if (!arp_cache[i].valid && slot < 0) {
            slot = i;
        }
    }
    
    if (slot < 0) slot = 0;  
    
    arp_cache[slot].ip = ip;
    memcpy(arp_cache[slot].mac, mac, 6);
    arp_cache[slot].valid = 1;
}

int arp_lookup(uint32_t ip, uint8_t *mac)
{
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy(mac, arp_cache[i].mac, 6);
            return 0;
        }
    }
    return -1; 
}

int arp_get_entry(int index, uint32_t *ip, uint8_t *mac)
{
    if (index < 0 || index >= ARP_CACHE_SIZE) return -1;
    if (!arp_cache[index].valid) return -1;
    
    *ip = arp_cache[index].ip;
    memcpy(mac, arp_cache[index].mac, 6);
    return 0;
}

void arp_request(uint32_t ip)
{
    netif_t *netif = net_get_default();
    if (!netif) return;
    
    arp_header_t arp;
    arp.htype = htons(ARP_HTYPE_ETH);
    arp.ptype = htons(ARP_PTYPE_IP);
    arp.hlen = 6;
    arp.plen = 4;
    arp.oper = htons(ARP_OP_REQUEST);
    memcpy(arp.sha, netif->mac, 6);
    arp.spa = htonl(netif->ip);
    memset(arp.tha, 0, 6);
    arp.tpa = htonl(ip);
    
    eth_send(broadcast_mac, ETH_TYPE_ARP, &arp, sizeof(arp));
    
    serial_puts("[ARP] Sent request\n");
}

void arp_receive(const void *packet, uint16_t len)
{
    if (len < sizeof(arp_header_t)) return;
    
    const arp_header_t *arp = (const arp_header_t *)packet;
    netif_t *netif = net_get_default();
    if (!netif) return;
    
    uint16_t oper = ntohs(arp->oper);
    uint32_t spa = ntohl(arp->spa);
    uint32_t tpa = ntohl(arp->tpa);
    
    arp_add_entry(spa, arp->sha);
    
    if (oper == ARP_OP_REQUEST && tpa == netif->ip) {
        arp_header_t reply;
        reply.htype = htons(ARP_HTYPE_ETH);
        reply.ptype = htons(ARP_PTYPE_IP);
        reply.hlen = 6;
        reply.plen = 4;
        reply.oper = htons(ARP_OP_REPLY);
        memcpy(reply.sha, netif->mac, 6);
        reply.spa = htonl(netif->ip);
        memcpy(reply.tha, arp->sha, 6);
        reply.tpa = arp->spa;
        
        eth_send(arp->sha, ETH_TYPE_ARP, &reply, sizeof(reply));
        serial_puts("[ARP] Sent reply\n");
    }
}

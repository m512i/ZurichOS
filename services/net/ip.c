/* IPv4
 * Internet Protocol version 4
 */

#include <net/ip.h>
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/net.h>
#include <drivers/serial.h>
#include <string.h>

static uint16_t ip_id = 0;

uint16_t ip_checksum(const void *data, uint16_t len)
{
    const uint16_t *ptr = (const uint16_t *)data;
    uint32_t sum = 0;
    
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    
    if (len == 1) {
        sum += *(const uint8_t *)ptr;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return (uint16_t)~sum;
}

void ip_send(uint32_t dest_ip, uint8_t protocol, const void *data, uint16_t len)
{
    netif_t *netif = net_get_default();
    if (!netif || netif->ip == 0) return;
    
    uint8_t packet[IP_MTU + IP_HEADER_LEN];
    ip_header_t *hdr = (ip_header_t *)packet;
    
    hdr->ihl_version = (IP_VERSION << 4) | (IP_HEADER_LEN / 4);
    hdr->tos = 0;
    hdr->total_len = htons(IP_HEADER_LEN + len);
    uint16_t pkt_id = ip_id++;
    hdr->id = htons(pkt_id);
    hdr->frag_off = 0;
    hdr->ttl = IP_TTL_DEFAULT;
    hdr->protocol = protocol;
    hdr->checksum = 0;
    hdr->src_ip = htonl(netif->ip);
    hdr->dest_ip = htonl(dest_ip);
    
    hdr->checksum = ip_checksum(hdr, IP_HEADER_LEN);
    
    if (len > IP_MTU) len = IP_MTU;
    memcpy(packet + IP_HEADER_LEN, data, len);
    
    uint32_t next_hop = dest_ip;
    if ((dest_ip & netif->netmask) != (netif->ip & netif->netmask)) {
        next_hop = netif->gateway;
    }
    
    uint8_t dest_mac[6];
    if (arp_lookup(next_hop, dest_mac) < 0) {
        arp_request(next_hop);
        
        extern void net_poll(void);
        for (int i = 0; i < 100000; i++) {
            net_poll();
            if (arp_lookup(next_hop, dest_mac) == 0) {
                break;
            }
        }
        
        if (arp_lookup(next_hop, dest_mac) < 0) {
            serial_puts("[IP] ARP resolution failed\n");
            return;
        }
    }
    
    eth_send(dest_mac, ETH_TYPE_IP, packet, IP_HEADER_LEN + len);
}

void ip_receive(const void *packet, uint16_t len)
{
    if (len < IP_HEADER_LEN) return;
    
    const ip_header_t *hdr = (const ip_header_t *)packet;
    
    if ((hdr->ihl_version >> 4) != IP_VERSION) return;
    
    uint16_t hdr_len = (hdr->ihl_version & 0x0F) * 4;
    
    netif_t *netif = net_get_default();
    uint32_t dest_ip = ntohl(hdr->dest_ip);
    uint32_t src_ip = ntohl(hdr->src_ip);
    
    if (netif && netif->ip != 0 && dest_ip != netif->ip && dest_ip != 0xFFFFFFFF) {
        return;  
    }
    
    const uint8_t *data = (const uint8_t *)packet + hdr_len;
    uint16_t data_len = ntohs(hdr->total_len) - hdr_len;
    
    switch (hdr->protocol) {
        case IP_PROTO_ICMP:
            icmp_receive(src_ip, data, data_len);
            break;
        case IP_PROTO_UDP:
            udp_receive(src_ip, data, data_len);
            break;
        case IP_PROTO_TCP:
            tcp_receive(src_ip, data, data_len);
            break;
        default:
            break;
    }
}

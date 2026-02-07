/* Ethernet
 * Ethernet frame handling
 */

#include <net/ethernet.h>
#include <net/net.h>
#include <net/arp.h>
#include <net/ip.h>
#include <drivers/serial.h>
#include <string.h>

void eth_send(const uint8_t *dest, uint16_t type, const void *data, uint16_t len)
{
    netif_t *netif = net_get_default();
    if (!netif) return;
    
    uint8_t frame[ETH_FRAME_LEN];
    eth_header_t *hdr = (eth_header_t *)frame;
    
    memcpy(hdr->dest, dest, ETH_ALEN);
    memcpy(hdr->src, netif->mac, ETH_ALEN);
    hdr->type = htons(type);
    
    if (len > ETH_DATA_LEN) len = ETH_DATA_LEN;
    memcpy(frame + ETH_HLEN, data, len);
    
    netif->send(frame, ETH_HLEN + len);
}

void eth_receive(const void *frame, uint16_t len)
{
    if (len < ETH_HLEN) return;
    
    const eth_header_t *hdr = (const eth_header_t *)frame;
    const uint8_t *data = (const uint8_t *)frame + ETH_HLEN;
    uint16_t data_len = len - ETH_HLEN;
    
    uint16_t type = ntohs(hdr->type);
    
    switch (type) {
        case ETH_TYPE_ARP:
            arp_receive(data, data_len);
            break;
        case ETH_TYPE_IP:
            ip_receive(data, data_len);
            break;
        default:
            break;
    }
}

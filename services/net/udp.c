/* UDP
 * User Datagram Protocol
 */

#include <net/udp.h>
#include <net/ip.h>
#include <net/net.h>
#include <drivers/serial.h>
#include <string.h>

int udp_send(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void *data, uint16_t len)
{
    uint8_t packet[1500];
    udp_header_t *hdr = (udp_header_t *)packet;
    
    hdr->src_port = htons(src_port);
    hdr->dest_port = htons(dest_port);
    hdr->length = htons(sizeof(udp_header_t) + len);
    hdr->checksum = 0;
    
    if (len > sizeof(packet) - sizeof(udp_header_t)) {
        len = sizeof(packet) - sizeof(udp_header_t);
    }
    
    memcpy(packet + sizeof(udp_header_t), data, len);
    
    ip_send(dest_ip, IP_PROTO_UDP, packet, sizeof(udp_header_t) + len);
    
    return len;
}

void udp_receive(uint32_t src_ip, const void *packet, uint16_t len)
{
    (void)src_ip;
    
    if (len < sizeof(udp_header_t)) return;
    
    const udp_header_t *hdr = (const udp_header_t *)packet;
    uint16_t src_port = ntohs(hdr->src_port);
    uint16_t dest_port = ntohs(hdr->dest_port);
    uint16_t data_len = ntohs(hdr->length) - sizeof(udp_header_t);
    
    const void *data = (const uint8_t *)packet + sizeof(udp_header_t);
    
    if (src_port == 53) {
        extern void dns_receive(const void *data, uint16_t len);
        dns_receive(data, data_len);
    } else if (src_port == 67 || dest_port == 68) {
        extern void dhcp_receive(const void *data, uint16_t len);
        dhcp_receive(data, data_len);
    }
    
    /* TODO: Deliver to socket for other ports */
}

/* ICMP
 * Internet Control Message Protocol (ping)
 */

#include <net/icmp.h>
#include <net/ip.h>
#include <net/net.h>
#include <drivers/serial.h>
#include <string.h>

static uint16_t icmp_checksum(const void *data, uint16_t len)
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

int icmp_send_echo_request(uint32_t dest_ip, uint16_t id, uint16_t seq, const void *data, uint16_t len)
{
    uint8_t packet[64];
    icmp_header_t *hdr = (icmp_header_t *)packet;
    
    hdr->type = ICMP_ECHO_REQUEST;
    hdr->code = 0;
    hdr->checksum = 0;
    hdr->id = htons(id);
    hdr->seq = htons(seq);
    
    if (len > sizeof(packet) - sizeof(icmp_header_t)) {
        len = sizeof(packet) - sizeof(icmp_header_t);
    }
    
    if (data && len > 0) {
        memcpy(packet + sizeof(icmp_header_t), data, len);
    }
    
    hdr->checksum = icmp_checksum(packet, sizeof(icmp_header_t) + len);
    
    ip_send(dest_ip, IP_PROTO_ICMP, packet, sizeof(icmp_header_t) + len);
    
    serial_puts("[ICMP] Sent echo request\n");
    return 0;
}

int icmp_send_echo_reply(uint32_t dest_ip, uint16_t id, uint16_t seq, const void *data, uint16_t len)
{
    uint8_t packet[64];
    icmp_header_t *hdr = (icmp_header_t *)packet;
    
    hdr->type = ICMP_ECHO_REPLY;
    hdr->code = 0;
    hdr->checksum = 0;
    hdr->id = htons(id);
    hdr->seq = htons(seq);
    
    if (len > sizeof(packet) - sizeof(icmp_header_t)) {
        len = sizeof(packet) - sizeof(icmp_header_t);
    }
    
    if (data && len > 0) {
        memcpy(packet + sizeof(icmp_header_t), data, len);
    }
    
    hdr->checksum = icmp_checksum(packet, sizeof(icmp_header_t) + len);
    
    ip_send(dest_ip, IP_PROTO_ICMP, packet, sizeof(icmp_header_t) + len);
    
    serial_puts("[ICMP] Sent echo reply\n");
    return 0;
}

void icmp_receive(uint32_t src_ip, const void *packet, uint16_t len)
{
    if (len < sizeof(icmp_header_t)) return;
    
    const icmp_header_t *hdr = (const icmp_header_t *)packet;
    
    switch (hdr->type) {
        case ICMP_ECHO_REQUEST:
            serial_puts("[ICMP] Received echo request\n");
            icmp_send_echo_reply(src_ip, ntohs(hdr->id), ntohs(hdr->seq),
                                 (const uint8_t *)packet + sizeof(icmp_header_t),
                                 len - sizeof(icmp_header_t));
            break;
            
        case ICMP_ECHO_REPLY:
            serial_puts("[ICMP] Received echo reply\n");
            extern void ping_set_reply(uint16_t bytes, uint32_t rtt, uint8_t ttl);
            ping_set_reply(len, 0, 64);
            break;
            
        default:
            break;
    }
}

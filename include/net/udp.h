/* UDP Header
 * User Datagram Protocol definitions
 */

#ifndef _NET_UDP_H
#define _NET_UDP_H

#include <stdint.h>

typedef struct udp_header {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

void udp_receive(uint32_t src_ip, const void *packet, uint16_t len);
int udp_send(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void *data, uint16_t len);

#endif 

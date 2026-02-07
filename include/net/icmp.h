/* ICMP Header
 * Internet Control Message Protocol definitions
 */

#ifndef _NET_ICMP_H
#define _NET_ICMP_H

#include <stdint.h>

#define ICMP_ECHO_REPLY     0
#define ICMP_DEST_UNREACH   3
#define ICMP_ECHO_REQUEST   8
#define ICMP_TIME_EXCEEDED  11

typedef struct icmp_header {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed)) icmp_header_t;

void icmp_receive(uint32_t src_ip, const void *packet, uint16_t len);
int icmp_send_echo_request(uint32_t dest_ip, uint16_t id, uint16_t seq, const void *data, uint16_t len);
int icmp_send_echo_reply(uint32_t dest_ip, uint16_t id, uint16_t seq, const void *data, uint16_t len);

#endif 

/* Network Core Header
 * Common definitions for networking stack
 */

#ifndef _NET_NET_H
#define _NET_NET_H

#include <stdint.h>

#define htons(x) ((uint16_t)(((uint16_t)(x) << 8) | ((uint16_t)(x) >> 8)))
#define ntohs(x) htons(x)
#define htonl(x) ((uint32_t)(((uint32_t)(x) << 24) | (((uint32_t)(x) & 0xFF00) << 8) | \
                             (((uint32_t)(x) >> 8) & 0xFF00) | ((uint32_t)(x) >> 24)))
#define ntohl(x) htonl(x)

#define ETH_MTU         1500
#define ETH_FRAME_MAX   1518
#define IP_MTU          1480

typedef struct netbuf {
    uint8_t *data;
    uint16_t len;
    uint16_t capacity;
    struct netbuf *next;
} netbuf_t;

typedef struct netif {
    char name[8];
    uint8_t mac[6];
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    int (*send)(const void *data, uint16_t len);
    int (*receive)(void *data, uint16_t max_len);
} netif_t;

void net_init(void);
netif_t *net_get_default(void);

void net_set_ip(uint32_t ip, uint32_t netmask, uint32_t gateway);

void net_poll(void);

#endif

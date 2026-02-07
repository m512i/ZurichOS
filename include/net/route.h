/* Routing Table Header
 * Static routes and gateway management
 */

#ifndef _NET_ROUTE_H
#define _NET_ROUTE_H

#include <stdint.h>

#define MAX_ROUTES 16

#define ROUTE_FLAG_UP       0x01
#define ROUTE_FLAG_GATEWAY  0x02
#define ROUTE_FLAG_HOST     0x04
#define ROUTE_FLAG_DEFAULT  0x08

typedef struct route_entry {
    uint32_t dest;          
    uint32_t netmask;       
    uint32_t gateway;       
    uint32_t metric;        
    uint8_t flags;
    uint8_t in_use;
    char iface[8];          
} route_entry_t;

void route_init(void);
int route_add(uint32_t dest, uint32_t netmask, uint32_t gateway, uint32_t metric, const char *iface);
int route_del(uint32_t dest, uint32_t netmask);
uint32_t route_lookup(uint32_t dest);
int route_get_count(void);
route_entry_t *route_get_entry(int index);
void route_print(void);

#endif

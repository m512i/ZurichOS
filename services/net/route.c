/* Routing Table
 * Static routes and gateway management
 */

#include <net/route.h>
#include <net/net.h>
#include <drivers/serial.h>
#include <string.h>

static route_entry_t routes[MAX_ROUTES];

void route_init(void)
{
    memset(routes, 0, sizeof(routes));
    serial_puts("[ROUTE] Initialized\n");
}

int route_add(uint32_t dest, uint32_t netmask, uint32_t gateway, uint32_t metric, const char *iface)
{
    for (int i = 0; i < MAX_ROUTES; i++) {
        if (!routes[i].in_use) {
            routes[i].dest = dest;
            routes[i].netmask = netmask;
            routes[i].gateway = gateway;
            routes[i].metric = metric;
            routes[i].flags = ROUTE_FLAG_UP;
            if (gateway != 0) routes[i].flags |= ROUTE_FLAG_GATEWAY;
            if (dest == 0 && netmask == 0) routes[i].flags |= ROUTE_FLAG_DEFAULT;
            routes[i].in_use = 1;
            
            if (iface) {
                strncpy(routes[i].iface, iface, 7);
                routes[i].iface[7] = '\0';
            } else {
                strcpy(routes[i].iface, "eth0");
            }
            
            serial_puts("[ROUTE] Added route\n");
            return 0;
        }
    }
    return -1;  
}

int route_del(uint32_t dest, uint32_t netmask)
{
    for (int i = 0; i < MAX_ROUTES; i++) {
        if (routes[i].in_use && routes[i].dest == dest && routes[i].netmask == netmask) {
            routes[i].in_use = 0;
            serial_puts("[ROUTE] Deleted route\n");
            return 0;
        }
    }
    return -1;
}

uint32_t route_lookup(uint32_t dest)
{
    uint32_t best_gateway = 0;
    uint32_t best_mask = 0;
    uint32_t best_metric = 0xFFFFFFFF;
    
    for (int i = 0; i < MAX_ROUTES; i++) {
        if (!routes[i].in_use) continue;
        if (!(routes[i].flags & ROUTE_FLAG_UP)) continue;
        
        if ((dest & routes[i].netmask) == (routes[i].dest & routes[i].netmask)) {
            if (routes[i].netmask >= best_mask) {
                if (routes[i].netmask > best_mask || routes[i].metric < best_metric) {
                    best_gateway = routes[i].gateway;
                    best_mask = routes[i].netmask;
                    best_metric = routes[i].metric;
                }
            }
        }
    }
    
    return best_gateway;
}

int route_get_count(void)
{
    int count = 0;
    for (int i = 0; i < MAX_ROUTES; i++) {
        if (routes[i].in_use) count++;
    }
    return count;
}

route_entry_t *route_get_entry(int index)
{
    int count = 0;
    for (int i = 0; i < MAX_ROUTES; i++) {
        if (routes[i].in_use) {
            if (count == index) return &routes[i];
            count++;
        }
    }
    return NULL;
}

void route_print(void)
{
    serial_puts("[ROUTE] Routing table:\n");
    for (int i = 0; i < MAX_ROUTES; i++) {
        if (routes[i].in_use) {
            serial_puts("  Route entry active\n");
        }
    }
}

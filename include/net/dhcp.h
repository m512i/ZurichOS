/* DHCP Client Header
 * Dynamic Host Configuration Protocol
 */

#ifndef _NET_DHCP_H
#define _NET_DHCP_H

#include <stdint.h>

#define DHCP_SERVER_PORT    67
#define DHCP_CLIENT_PORT    68

#define DHCP_DISCOVER   1
#define DHCP_OFFER      2
#define DHCP_REQUEST    3
#define DHCP_DECLINE    4
#define DHCP_ACK        5
#define DHCP_NAK        6
#define DHCP_RELEASE    7
#define DHCP_INFORM     8

#define DHCP_OPT_PAD            0
#define DHCP_OPT_SUBNET_MASK    1
#define DHCP_OPT_ROUTER         3
#define DHCP_OPT_DNS            6
#define DHCP_OPT_HOSTNAME       12
#define DHCP_OPT_REQUESTED_IP   50
#define DHCP_OPT_LEASE_TIME     51
#define DHCP_OPT_MSG_TYPE       53
#define DHCP_OPT_SERVER_ID      54
#define DHCP_OPT_PARAM_REQ      55
#define DHCP_OPT_END            255

#define DHCP_MAGIC_COOKIE   0x63825363

typedef struct dhcp_header {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;           
    uint32_t xid;           
    uint16_t secs;          
    uint16_t flags;        
    uint32_t ciaddr;       
    uint32_t yiaddr;        
    uint32_t siaddr;       
    uint32_t giaddr;       
    uint8_t chaddr[16];     
    uint8_t sname[64];      
    uint8_t file[128];      
    uint32_t magic;         
    uint8_t options[308];   
} __attribute__((packed)) dhcp_header_t;

typedef enum {
    DHCP_STATE_INIT,
    DHCP_STATE_SELECTING,
    DHCP_STATE_REQUESTING,
    DHCP_STATE_BOUND,
    DHCP_STATE_RENEWING,
    DHCP_STATE_REBINDING
} dhcp_state_t;

typedef struct dhcp_lease {
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns;
    uint32_t server;
    uint32_t lease_time;
    uint32_t obtained;
} dhcp_lease_t;

void dhcp_init(void);
int dhcp_discover(void);
int dhcp_request(uint32_t requested_ip, uint32_t server_ip);
void dhcp_receive(const void *data, uint16_t len);
dhcp_state_t dhcp_get_state(void);
dhcp_lease_t *dhcp_get_lease(void);

#endif

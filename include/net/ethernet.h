/* Ethernet Header
 * Ethernet frame definitions
 */

#ifndef _NET_ETHERNET_H
#define _NET_ETHERNET_H

#include <stdint.h>

#define ETH_ALEN        6       
#define ETH_HLEN        14      
#define ETH_DATA_LEN    1500    
#define ETH_FRAME_LEN   1514    

#define ETH_TYPE_IP     0x0800
#define ETH_TYPE_ARP    0x0806
#define ETH_TYPE_IPV6   0x86DD

typedef struct eth_header {
    uint8_t dest[ETH_ALEN];     
    uint8_t src[ETH_ALEN];      
    uint16_t type;              
} __attribute__((packed)) eth_header_t;

typedef struct eth_frame {
    eth_header_t header;
    uint8_t data[ETH_DATA_LEN];
} __attribute__((packed)) eth_frame_t;

void eth_send(const uint8_t *dest, uint16_t type, const void *data, uint16_t len);
void eth_receive(const void *frame, uint16_t len);

#endif

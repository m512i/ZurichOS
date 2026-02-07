/* Loopback Interface Header
 * 127.0.0.1 support
 */

#ifndef _NET_LOOPBACK_H
#define _NET_LOOPBACK_H

#include <stdint.h>
#include <net/net.h>

#define LOOPBACK_IP     0x7F000001  
#define LOOPBACK_MASK   0xFF000000  

void loopback_init(void);
int loopback_send(const void *data, uint16_t len);
int loopback_receive(void *data, uint16_t max_len);
int is_loopback_address(uint32_t ip);
netif_t *loopback_get_interface(void);

#endif

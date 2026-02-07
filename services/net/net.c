/* Network Core
 * Ring 2 service for networking
 */

#include <net/net.h>
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/ip.h>
#include <drivers/e1000.h>
#include <drivers/serial.h>
#include <string.h>

static netif_t default_netif;
static uint8_t rx_buffer[2048];

void net_init(void)
{
    serial_puts("[NET] Initializing network stack...\n");
    
    if (e1000_init() < 0) {
        serial_puts("[NET] No network device found\n");
        return;
    }
    
    strcpy(default_netif.name, "eth0");
    e1000_get_mac(default_netif.mac);
    default_netif.ip = 0;
    default_netif.netmask = 0;
    default_netif.gateway = 0;
    default_netif.send = e1000_send;
    default_netif.receive = e1000_receive;
    
    arp_init();
    
    serial_puts("[NET] Network stack initialized\n");
}

netif_t *net_get_default(void)
{
    return &default_netif;
}

void net_set_ip(uint32_t ip, uint32_t netmask, uint32_t gateway)
{
    default_netif.ip = ip;
    default_netif.netmask = netmask;
    default_netif.gateway = gateway;
    
    serial_puts("[NET] IP configured: ");
    char buf[16];
    int idx = 0;
    for (int i = 3; i >= 0; i--) {
        uint8_t octet = (ip >> (i * 8)) & 0xFF;
        if (octet >= 100) buf[idx++] = '0' + octet / 100;
        if (octet >= 10) buf[idx++] = '0' + (octet / 10) % 10;
        buf[idx++] = '0' + octet % 10;
        if (i > 0) buf[idx++] = '.';
    }
    buf[idx] = '\0';
    serial_puts(buf);
    serial_puts("\n");
}

void net_poll(void)
{
    int len;
    int count = 0;
    while ((len = default_netif.receive(rx_buffer, sizeof(rx_buffer))) > 0 && count < 16) {
        eth_receive(rx_buffer, len);
        count++;
    }
}

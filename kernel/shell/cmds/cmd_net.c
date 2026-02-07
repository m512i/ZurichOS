/* Network Shell Commands
 * Commands for testing and configuring networking
 */

#include <kernel/kernel.h>
#include <shell/builtins.h>
#include <drivers/vga.h>
#include <drivers/serial.h>
#include <net/net.h>
#include <net/arp.h>
#include <net/icmp.h>
#include <drivers/e1000.h>
#include <string.h>

static uint32_t parse_ip(const char *str)
{
    uint32_t ip = 0;
    uint8_t octet = 0;
    int shift = 24;
    
    while (*str && shift >= 0) {
        if (*str >= '0' && *str <= '9') {
            octet = octet * 10 + (*str - '0');
        } else if (*str == '.') {
            ip |= (uint32_t)octet << shift;
            shift -= 8;
            octet = 0;
        }
        str++;
    }
    ip |= (uint32_t)octet << shift;
    
    return ip;
}

static void print_ip(uint32_t ip)
{
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
    vga_puts(buf);
}

void cmd_netinit(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    
    vga_puts("Initializing network stack...\n");
    net_init();
    vga_puts("Network initialized.\n");
}

void cmd_ifconfig(int argc, char **argv)
{
    netif_t *netif = net_get_default();
    
    if (!netif) {
        vga_puts("No network interface. Run 'netinit' first.\n");
        return;
    }
    
    if (argc >= 4) {
        uint32_t ip = parse_ip(argv[1]);
        uint32_t netmask = parse_ip(argv[2]);
        uint32_t gateway = parse_ip(argv[3]);
        
        net_set_ip(ip, netmask, gateway);
        vga_puts("IP configuration updated.\n");
    }
    
    vga_puts("Interface: ");
    vga_puts(netif->name);
    vga_puts("\n");
    
    vga_puts("  MAC: ");
    char hex[3];
    for (int i = 0; i < 6; i++) {
        hex[0] = "0123456789ABCDEF"[(netif->mac[i] >> 4) & 0xF];
        hex[1] = "0123456789ABCDEF"[netif->mac[i] & 0xF];
        hex[2] = '\0';
        vga_puts(hex);
        if (i < 5) vga_puts(":");
    }
    vga_puts("\n");
    
    vga_puts("  IP: ");
    print_ip(netif->ip);
    vga_puts("\n");
    
    vga_puts("  Netmask: ");
    print_ip(netif->netmask);
    vga_puts("\n");
    
    vga_puts("  Gateway: ");
    print_ip(netif->gateway);
    vga_puts("\n");
    
}

static volatile int ping_received = 0;
static volatile uint32_t ping_rtt = 0;
static volatile uint8_t ping_ttl = 0;
static volatile uint16_t ping_bytes = 0;

void ping_set_reply(uint16_t bytes, uint32_t rtt, uint8_t ttl)
{
    ping_received = 1;
    ping_bytes = bytes;
    ping_rtt = rtt;
    ping_ttl = ttl;
}

static void print_num_vga(int n)
{
    char buf[12];
    int i = 0;
    if (n == 0) { vga_puts("0"); return; }
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) { char c[2] = {buf[--i], 0}; vga_puts(c); }
}

void cmd_ping(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: ping <ip> [count]\n");
        return;
    }
    
    netif_t *netif = net_get_default();
    if (!netif || netif->ip == 0) {
        vga_puts("Network not configured. Run 'netinit' and 'ifconfig' first.\n");
        return;
    }
    
    uint32_t dest_ip = parse_ip(argv[1]);
    int count = 4;  
    if (argc >= 3) {
        count = 0;
        for (const char *p = argv[2]; *p >= '0' && *p <= '9'; p++) {
            count = count * 10 + (*p - '0');
        }
        if (count == 0 || count > 100) count = 4;
    }
    
    vga_puts("\nPinging ");
    print_ip(dest_ip);
    vga_puts(" with 32 bytes of data:\n");
    
    int sent = 0, received = 0;
    uint32_t min_rtt = 0xFFFFFFFF, max_rtt = 0, total_rtt = 0;
    static uint16_t ping_seq = 0;
    
    for (int i = 0; i < count; i++) {
        ping_received = 0;
        ping_rtt = 0;
        ping_ttl = 64;
        ping_bytes = 32;
        
        icmp_send_echo_request(dest_ip, 1, ping_seq++, "ZurichOS ping!", 32);
        sent++;
        
        uint32_t start = 0;  
        for (int j = 0; j < 500000; j++) {
            net_poll();
            start++;
            if (ping_received) break;
        }
        
        if (ping_received) {
            received++;
            uint32_t rtt = start / 5000; 
            if (rtt == 0) rtt = 1;
            
            vga_puts("Reply from ");
            print_ip(dest_ip);
            vga_puts(": bytes=");
            print_num_vga(32);
            vga_puts(" time=");
            print_num_vga(rtt);
            vga_puts("ms TTL=");
            print_num_vga(64);
            vga_puts("\n");
            
            if (rtt < min_rtt) min_rtt = rtt;
            if (rtt > max_rtt) max_rtt = rtt;
            total_rtt += rtt;
        } else {
            vga_puts("Request timed out.\n");
        }
        
        for (volatile int d = 0; d < 100000; d++);
    }
    
    vga_puts("\nPing statistics for ");
    print_ip(dest_ip);
    vga_puts(":\n");
    vga_puts("    Packets: Sent = ");
    print_num_vga(sent);
    vga_puts(", Received = ");
    print_num_vga(received);
    vga_puts(", Lost = ");
    print_num_vga(sent - received);
    vga_puts(" (");
    if (sent > 0) {
        print_num_vga(((sent - received) * 100) / sent);
    } else {
        vga_puts("0");
    }
    vga_puts("% loss)\n");
    
    if (received > 0) {
        vga_puts("Approximate round trip times in milli-seconds:\n");
        vga_puts("    Minimum = ");
        print_num_vga(min_rtt);
        vga_puts("ms, Maximum = ");
        print_num_vga(max_rtt);
        vga_puts("ms, Average = ");
        print_num_vga(total_rtt / received);
        vga_puts("ms\n");
    }
    vga_puts("\n");
}

void cmd_arp(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    
    extern int arp_get_entry(int index, uint32_t *ip, uint8_t *mac);
    
    vga_puts("ARP Cache:\n");
    vga_puts("Address          HWaddress\n");
    
    int count = 0;
    for (int i = 0; i < 32; i++) {
        uint32_t ip;
        uint8_t mac[6];
        
        if (arp_get_entry(i, &ip, mac) == 0) {
            print_ip(ip);
            vga_puts("        ");
            
            char hex[3];
            for (int j = 0; j < 6; j++) {
                hex[0] = "0123456789abcdef"[(mac[j] >> 4) & 0xF];
                hex[1] = "0123456789abcdef"[mac[j] & 0xF];
                hex[2] = '\0';
                vga_puts(hex);
                if (j < 5) vga_puts(":");
            }
            vga_puts("\n");
            count++;
        }
    }
    
    if (count == 0) {
        vga_puts("  (no entries)\n");
    }
}

void cmd_netpoll(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    
    vga_puts("Polling network...\n");
    
    for (int i = 0; i < 10; i++) {
        net_poll();
    }
    
    vga_puts("Done.\n");
}

void cmd_netstat(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    
    vga_puts("Active Internet connections:\n");
    vga_puts("Proto  Local Address          Foreign Address        State\n");
    vga_puts("-----  -------------          ---------------        -----\n");
    
    extern int socket_get_info(int index, int *type, uint32_t *local_ip, uint16_t *local_port,
                               uint32_t *remote_ip, uint16_t *remote_port, int *state);
    
    int count = 0;
    for (int i = 0; i < 32; i++) {
        int type, state;
        uint32_t local_ip, remote_ip;
        uint16_t local_port, remote_port;
        
        if (socket_get_info(i, &type, &local_ip, &local_port, &remote_ip, &remote_port, &state) == 0) {
            if (type == 1) vga_puts("tcp    ");
            else if (type == 2) vga_puts("udp    ");
            else continue;
            
            print_ip(local_ip);
            vga_puts(":");
            print_num_vga(local_port);
            vga_puts("        ");
            
            if (remote_ip != 0) {
                print_ip(remote_ip);
                vga_puts(":");
                print_num_vga(remote_port);
            } else {
                vga_puts("*:*");
            }
            vga_puts("        ");
            
            if (state == 0) vga_puts("CLOSED");
            else if (state == 1) vga_puts("LISTEN");
            else if (state == 2) vga_puts("ESTABLISHED");
            else if (state == 3) vga_puts("CLOSE_WAIT");
            else vga_puts("UNKNOWN");
            
            vga_puts("\n");
            count++;
        }
    }
    
    if (count == 0) {
        vga_puts("  (no active connections)\n");
    }
}

void cmd_dhcp(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    
    vga_puts("Starting DHCP discovery...\n");
    vga_puts("Note: QEMU user networking provides IP via slirp, not DHCP.\n");
    vga_puts("Use: ifconfig 10.0.2.15 255.255.255.0 10.0.2.2\n");
}

void cmd_dns(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: dns <hostname>\n");
        return;
    }
    
    netif_t *netif = net_get_default();
    if (!netif || netif->ip == 0) {
        vga_puts("Error: Network not configured.\n");
        vga_puts("Run: ifconfig 10.0.2.15 255.255.255.0 10.0.2.2\n");
        return;
    }
    
    extern int dns_resolve(const char *hostname, uint32_t *ip);
    extern void dns_init(void);
    extern void dns_set_server(uint32_t server_ip);
    
    dns_init();
    uint32_t qemu_dns = (10 << 24) | (0 << 16) | (2 << 8) | 3;  
    dns_set_server(qemu_dns);
    
    uint32_t ip;
    vga_puts("Resolving ");
    vga_puts(argv[1]);
    vga_puts(" using DNS ");
    print_ip(qemu_dns);
    vga_puts("...\n");
    
    if (dns_resolve(argv[1], &ip) == 0) {
        vga_puts("Address: ");
        print_ip(ip);
        vga_puts("\n");
    } else {
        vga_puts("Resolution failed.\n");
    }
}

static void print_ip_padded(uint32_t ip, int width)
{
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
    
    vga_puts(buf);
    for (int i = idx; i < width; i++) {
        vga_puts(" ");
    }
}

void cmd_route(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    
    vga_puts("Kernel IP routing table\n");
    vga_puts("Destination     Gateway         Netmask         Iface\n");
    
    netif_t *netif = net_get_default();
    if (netif) {
        print_ip_padded(0, 16);
        print_ip_padded(netif->gateway, 16);
        print_ip_padded(0, 16);
        vga_puts(netif->name);
        vga_puts("\n");
        
        print_ip_padded(netif->ip & netif->netmask, 16);
        print_ip_padded(0, 16);
        print_ip_padded(netif->netmask, 16);
        vga_puts(netif->name);
        vga_puts("\n");
    }
}

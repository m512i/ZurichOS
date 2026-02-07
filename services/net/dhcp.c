/* DHCP Client
 * Dynamic Host Configuration Protocol
 */

#include <net/dhcp.h>
#include <net/net.h>
#include <net/udp.h>
#include <net/ip.h>
#include <drivers/serial.h>
#include <string.h>

static dhcp_state_t dhcp_state = DHCP_STATE_INIT;
static dhcp_lease_t dhcp_lease;
static uint32_t dhcp_xid = 0x12345678;

static void dhcp_add_option(uint8_t *options, int *offset, uint8_t type, uint8_t len, const void *data)
{
    options[(*offset)++] = type;
    options[(*offset)++] = len;
    memcpy(&options[*offset], data, len);
    *offset += len;
}

void dhcp_init(void)
{
    memset(&dhcp_lease, 0, sizeof(dhcp_lease));
    dhcp_state = DHCP_STATE_INIT;
    serial_puts("[DHCP] Initialized\n");
}

int dhcp_discover(void)
{
    netif_t *netif = net_get_default();
    if (!netif) return -1;
    
    dhcp_header_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    
    pkt.op = 1; 
    pkt.htype = 1;  
    pkt.hlen = 6;
    uint32_t xid = dhcp_xid++;
    pkt.xid = htonl(xid);
    pkt.flags = htons(0x8000);  
    pkt.magic = htonl(DHCP_MAGIC_COOKIE);
    
    memcpy(pkt.chaddr, netif->mac, 6);
    
    int opt_offset = 0;
    uint8_t msg_type = DHCP_DISCOVER;
    dhcp_add_option(pkt.options, &opt_offset, DHCP_OPT_MSG_TYPE, 1, &msg_type);
    
    uint8_t param_req[] = {DHCP_OPT_SUBNET_MASK, DHCP_OPT_ROUTER, DHCP_OPT_DNS};
    dhcp_add_option(pkt.options, &opt_offset, DHCP_OPT_PARAM_REQ, 3, param_req);
    
    pkt.options[opt_offset++] = DHCP_OPT_END;
    
    udp_send(0xFFFFFFFF, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, &pkt, sizeof(pkt));
    
    dhcp_state = DHCP_STATE_SELECTING;
    serial_puts("[DHCP] Sent DISCOVER\n");
    
    return 0;
}

int dhcp_request(uint32_t requested_ip, uint32_t server_ip)
{
    netif_t *netif = net_get_default();
    if (!netif) return -1;
    
    dhcp_header_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    
    pkt.op = 1;
    pkt.htype = 1;
    pkt.hlen = 6;
    uint32_t xid = dhcp_xid++;
    pkt.xid = htonl(xid);
    pkt.flags = htons(0x8000);
    pkt.magic = htonl(DHCP_MAGIC_COOKIE);
    
    memcpy(pkt.chaddr, netif->mac, 6);
    
    int opt_offset = 0;
    uint8_t msg_type = DHCP_REQUEST;
    dhcp_add_option(pkt.options, &opt_offset, DHCP_OPT_MSG_TYPE, 1, &msg_type);
    
    uint32_t req_ip = htonl(requested_ip);
    dhcp_add_option(pkt.options, &opt_offset, DHCP_OPT_REQUESTED_IP, 4, &req_ip);
    
    uint32_t srv_ip = htonl(server_ip);
    dhcp_add_option(pkt.options, &opt_offset, DHCP_OPT_SERVER_ID, 4, &srv_ip);
    
    pkt.options[opt_offset++] = DHCP_OPT_END;
    
    udp_send(0xFFFFFFFF, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, &pkt, sizeof(pkt));
    
    dhcp_state = DHCP_STATE_REQUESTING;
    serial_puts("[DHCP] Sent REQUEST\n");
    
    return 0;
}

void dhcp_receive(const void *data, uint16_t len)
{
    if (len < sizeof(dhcp_header_t) - 308) return;
    
    const dhcp_header_t *pkt = (const dhcp_header_t *)data;
    
    if (pkt->op != 2) return;  
    if (ntohl(pkt->magic) != DHCP_MAGIC_COOKIE) return;
    
    uint8_t msg_type = 0;
    uint32_t server_id = 0;
    uint32_t subnet = 0;
    uint32_t router = 0;
    uint32_t dns = 0;
    uint32_t lease_time = 0;
    
    const uint8_t *opt = pkt->options;
    while (*opt != DHCP_OPT_END && opt < pkt->options + 308) {
        uint8_t type = *opt++;
        if (type == DHCP_OPT_PAD) continue;
        
        uint8_t olen = *opt++;
        
        switch (type) {
            case DHCP_OPT_MSG_TYPE:
                msg_type = *opt;
                break;
            case DHCP_OPT_SERVER_ID:
                memcpy(&server_id, opt, 4);
                server_id = ntohl(server_id);
                break;
            case DHCP_OPT_SUBNET_MASK:
                memcpy(&subnet, opt, 4);
                subnet = ntohl(subnet);
                break;
            case DHCP_OPT_ROUTER:
                memcpy(&router, opt, 4);
                router = ntohl(router);
                break;
            case DHCP_OPT_DNS:
                memcpy(&dns, opt, 4);
                dns = ntohl(dns);
                break;
            case DHCP_OPT_LEASE_TIME:
                memcpy(&lease_time, opt, 4);
                lease_time = ntohl(lease_time);
                break;
        }
        opt += olen;
    }
    
    uint32_t offered_ip = ntohl(pkt->yiaddr);
    
    if (msg_type == DHCP_OFFER && dhcp_state == DHCP_STATE_SELECTING) {
        serial_puts("[DHCP] Received OFFER: ");
        dhcp_lease.ip = offered_ip;
        dhcp_lease.server = server_id;
        dhcp_request(offered_ip, server_id);
    }
    else if (msg_type == DHCP_ACK && dhcp_state == DHCP_STATE_REQUESTING) {
        serial_puts("[DHCP] Received ACK\n");
        dhcp_lease.ip = offered_ip;
        dhcp_lease.netmask = subnet;
        dhcp_lease.gateway = router;
        dhcp_lease.dns = dns;
        dhcp_lease.lease_time = lease_time;
        
        net_set_ip(dhcp_lease.ip, dhcp_lease.netmask, dhcp_lease.gateway);
        
        dhcp_state = DHCP_STATE_BOUND;
        serial_puts("[DHCP] Lease obtained\n");
    }
    else if (msg_type == DHCP_NAK) {
        serial_puts("[DHCP] Received NAK\n");
        dhcp_state = DHCP_STATE_INIT;
    }
}

dhcp_state_t dhcp_get_state(void)
{
    return dhcp_state;
}

dhcp_lease_t *dhcp_get_lease(void)
{
    return &dhcp_lease;
}

/* TCP
 * Transmission Control Protocol (simplified)
 */

#include <net/tcp.h>
#include <net/ip.h>
#include <net/net.h>
#include <mm/heap.h>
#include <drivers/serial.h>
#include <string.h>

#define MAX_TCP_CONNECTIONS 16

static tcp_conn_t connections[MAX_TCP_CONNECTIONS];

void tcp_init(void)
{
    memset(connections, 0, sizeof(connections));
    serial_puts("[TCP] Initialized\n");
}

static tcp_conn_t *tcp_alloc_conn(void)
{
    for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
        if (!connections[i].in_use) {
            memset(&connections[i], 0, sizeof(tcp_conn_t));
            connections[i].in_use = 1;
            return &connections[i];
        }
    }
    return NULL;
}

static tcp_conn_t *tcp_find_conn(uint32_t remote_ip, uint16_t remote_port, uint16_t local_port)
{
    for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
        if (connections[i].in_use &&
            connections[i].remote_ip == remote_ip &&
            connections[i].remote_port == remote_port &&
            connections[i].local_port == local_port) {
            return &connections[i];
        }
    }
    return NULL;
}

static uint16_t tcp_checksum(uint32_t src_ip, uint32_t dest_ip, const void *data, uint16_t len)
{
    uint32_t sum = 0;
    const uint16_t *ptr = (const uint16_t *)data;
    
    sum += (src_ip >> 16) & 0xFFFF;
    sum += src_ip & 0xFFFF;
    sum += (dest_ip >> 16) & 0xFFFF;
    sum += dest_ip & 0xFFFF;
    sum += htons(IP_PROTO_TCP);
    sum += htons(len);
    
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(const uint8_t *)ptr;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return (uint16_t)~sum;
}

static void tcp_send_packet(tcp_conn_t *conn, uint8_t flags, const void *data, uint16_t len)
{
    netif_t *netif = net_get_default();
    if (!netif) return;
    
    uint8_t packet[1500];
    tcp_header_t *hdr = (tcp_header_t *)packet;
    
    hdr->src_port = htons(conn->local_port);
    hdr->dest_port = htons(conn->remote_port);
    hdr->seq_num = htonl(conn->seq);
    hdr->ack_num = htonl(conn->ack);
    hdr->data_offset = (5 << 4);  
    hdr->flags = flags;
    hdr->window = htons(8192);
    hdr->checksum = 0;
    hdr->urgent_ptr = 0;
    
    if (len > 0 && data) {
        memcpy(packet + sizeof(tcp_header_t), data, len);
    }
    
    hdr->checksum = tcp_checksum(netif->ip, conn->remote_ip, packet, sizeof(tcp_header_t) + len);
    
    ip_send(conn->remote_ip, IP_PROTO_TCP, packet, sizeof(tcp_header_t) + len);
}

tcp_conn_t *tcp_connect(uint32_t dest_ip, uint16_t dest_port)
{
    tcp_conn_t *conn = tcp_alloc_conn();
    if (!conn) return NULL;
    
    netif_t *netif = net_get_default();
    if (!netif) return NULL;
    
    conn->local_ip = netif->ip;
    conn->remote_ip = dest_ip;
    conn->local_port = 49152 + (dest_port % 16384);
    conn->remote_port = dest_port;
    conn->seq = 1000;  /* Should be random */
    conn->ack = 0;
    conn->state = TCP_SYN_SENT;
    
    tcp_send_packet(conn, TCP_SYN, NULL, 0);
    conn->seq++;
    
    serial_puts("[TCP] Connecting...\n");
    return conn;
}

tcp_conn_t *tcp_listen(uint16_t port)
{
    tcp_conn_t *conn = tcp_alloc_conn();
    if (!conn) return NULL;
    
    netif_t *netif = net_get_default();
    conn->local_ip = netif ? netif->ip : 0;
    conn->local_port = port;
    conn->state = TCP_LISTEN;
    
    serial_puts("[TCP] Listening on port\n");
    return conn;
}

tcp_conn_t *tcp_accept(tcp_conn_t *listener)
{
    (void)listener;
    /* Simplified - would need to check for incoming SYN */
    return NULL;
}

int tcp_send(tcp_conn_t *conn, const void *data, uint16_t len)
{
    if (!conn || conn->state != TCP_ESTABLISHED) return -1;
    
    tcp_send_packet(conn, TCP_ACK | TCP_PSH, data, len);
    conn->seq += len;
    
    return len;
}

int tcp_recv(tcp_conn_t *conn, void *buffer, uint16_t max_len)
{
    if (!conn || !conn->recv_buf || conn->recv_len == 0) return 0;
    
    uint16_t len = conn->recv_len < max_len ? conn->recv_len : max_len;
    memcpy(buffer, conn->recv_buf, len);
    conn->recv_len = 0;
    
    return len;
}

void tcp_close(tcp_conn_t *conn)
{
    if (!conn) return;
    
    if (conn->state == TCP_ESTABLISHED) {
        tcp_send_packet(conn, TCP_FIN | TCP_ACK, NULL, 0);
        conn->state = TCP_FIN_WAIT_1;
    }
    
    conn->in_use = 0;
    serial_puts("[TCP] Connection closed\n");
}

void tcp_receive(uint32_t src_ip, const void *packet, uint16_t len)
{
    if (len < sizeof(tcp_header_t)) return;
    
    const tcp_header_t *hdr = (const tcp_header_t *)packet;
    uint16_t src_port = ntohs(hdr->src_port);
    uint16_t dest_port = ntohs(hdr->dest_port);
    uint32_t seq = ntohl(hdr->seq_num);
    (void)ntohl(hdr->ack_num);  /* ack_num used in state machine */
    uint8_t flags = hdr->flags;
    
    tcp_conn_t *conn = tcp_find_conn(src_ip, src_port, dest_port);
    
    if (!conn) {
        /* Check for listening socket */
        for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
            if (connections[i].in_use &&
                connections[i].state == TCP_LISTEN &&
                connections[i].local_port == dest_port) {
                conn = &connections[i];
                break;
            }
        }
    }
    
    if (!conn) return;
    
    /* State machine */
    switch (conn->state) {
        case TCP_LISTEN:
            if (flags & TCP_SYN) {
                conn->remote_ip = src_ip;
                conn->remote_port = src_port;
                conn->ack = seq + 1;
                conn->seq = 2000;
                conn->state = TCP_SYN_RECEIVED;
                tcp_send_packet(conn, TCP_SYN | TCP_ACK, NULL, 0);
                conn->seq++;
            }
            break;
            
        case TCP_SYN_SENT:
            if ((flags & TCP_SYN) && (flags & TCP_ACK)) {
                conn->ack = seq + 1;
                conn->state = TCP_ESTABLISHED;
                tcp_send_packet(conn, TCP_ACK, NULL, 0);
                serial_puts("[TCP] Connected\n");
            }
            break;
            
        case TCP_SYN_RECEIVED:
            if (flags & TCP_ACK) {
                conn->state = TCP_ESTABLISHED;
                serial_puts("[TCP] Connection established\n");
            }
            break;
            
        case TCP_ESTABLISHED:
            if (flags & TCP_FIN) {
                conn->ack = seq + 1;
                tcp_send_packet(conn, TCP_ACK, NULL, 0);
                conn->state = TCP_CLOSE_WAIT;
            } else if (flags & TCP_ACK) {
                uint16_t data_offset = ((hdr->data_offset >> 4) & 0xF) * 4;
                uint16_t data_len = len - data_offset;
                if (data_len > 0) {
                    conn->ack = seq + data_len;
                    tcp_send_packet(conn, TCP_ACK, NULL, 0);
                }
            }
            break;
            
        default:
            break;
    }
}

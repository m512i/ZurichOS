/* TCP Header
 * Transmission Control Protocol definitions
 */

#ifndef _NET_TCP_H
#define _NET_TCP_H

#include <stdint.h>

#define TCP_FIN     0x01
#define TCP_SYN     0x02
#define TCP_RST     0x04
#define TCP_PSH     0x08
#define TCP_ACK     0x10
#define TCP_URG     0x20

typedef enum {
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT
} tcp_state_t;

typedef struct tcp_header {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t data_offset;  
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed)) tcp_header_t;

typedef struct tcp_conn {
    uint32_t local_ip;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t seq;
    uint32_t ack;
    tcp_state_t state;
    uint8_t *recv_buf;
    uint16_t recv_len;
    uint8_t in_use;
} tcp_conn_t;

void tcp_init(void);
void tcp_receive(uint32_t src_ip, const void *packet, uint16_t len);
tcp_conn_t *tcp_connect(uint32_t dest_ip, uint16_t dest_port);
tcp_conn_t *tcp_listen(uint16_t port);
tcp_conn_t *tcp_accept(tcp_conn_t *listener);
int tcp_send(tcp_conn_t *conn, const void *data, uint16_t len);
int tcp_recv(tcp_conn_t *conn, void *buffer, uint16_t max_len);
void tcp_close(tcp_conn_t *conn);

#endif

/* Socket API Header
 * BSD-style socket interface
 */

#ifndef _NET_SOCKET_H
#define _NET_SOCKET_H

#include <stdint.h>

#define SOCK_STREAM     1   
#define SOCK_DGRAM      2   
#define SOCK_RAW        3   

#define AF_INET         2   

typedef struct sockaddr {
    uint16_t sa_family;
    char sa_data[14];
} sockaddr_t;

typedef struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t sin_zero[8];
} sockaddr_in_t;

#define MAX_SOCKETS 32

typedef struct socket {
    int type;               
    int protocol;           
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;
    void *conn;             
    uint8_t domain;
    uint8_t bound;
    uint8_t connected;
    uint8_t listening;
    uint8_t in_use;
} socket_t;

int socket_create(int domain, int type, int protocol);
int socket_bind(int sockfd, const sockaddr_t *addr, uint32_t addrlen);
int socket_listen(int sockfd, int backlog);
int socket_accept(int sockfd, sockaddr_t *addr, uint32_t *addrlen);
int socket_connect(int sockfd, const sockaddr_t *addr, uint32_t addrlen);
int socket_send(int sockfd, const void *buf, uint32_t len, int flags);
int socket_recv(int sockfd, void *buf, uint32_t len, int flags);
int socket_sendto(int sockfd, const void *buf, uint32_t len, int flags,
                  const sockaddr_t *dest_addr, uint32_t addrlen);
int socket_recvfrom(int sockfd, void *buf, uint32_t len, int flags,
                    sockaddr_t *src_addr, uint32_t *addrlen);
int socket_close(int sockfd);
int socket_shutdown(int sockfd, int how);
int socket_getpeername(int sockfd, sockaddr_t *addr, uint32_t *addrlen);
int socket_getsockname(int sockfd, sockaddr_t *addr, uint32_t *addrlen);
int socket_setsockopt(int sockfd, int level, int optname, const void *optval, uint32_t optlen);
int socket_getsockopt(int sockfd, int level, int optname, void *optval, uint32_t *optlen);
int socket_select(int nfds, uint32_t *readfds, uint32_t *writefds, uint32_t *exceptfds, uint32_t timeout_ms);

#define SHUT_RD     0
#define SHUT_WR     1
#define SHUT_RDWR   2

#define SOL_SOCKET      1
#define IPPROTO_TCP     6
#define SO_REUSEADDR    2
#define SO_KEEPALIVE    9
#define TCP_NODELAY     1

void socket_init(void);

#endif 

/* Socket API
 * BSD-style socket interface
 */

#include <net/socket.h>
#include <net/net.h>
#include <net/ip.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <drivers/serial.h>
#include <string.h>

static socket_t sockets[MAX_SOCKETS];

void socket_init(void)
{
    memset(sockets, 0, sizeof(sockets));
    tcp_init();
    serial_puts("[SOCKET] Initialized\n");
}

static socket_t *get_socket(int fd)
{
    if (fd < 0 || fd >= MAX_SOCKETS) return NULL;
    if (!sockets[fd].in_use) return NULL;
    return &sockets[fd];
}

int socket_create(int domain, int type, int protocol)
{
    (void)protocol;
    
    if (domain != AF_INET) {
        return -1;  
    }
    
    if (type != SOCK_STREAM && type != SOCK_DGRAM) {
        return -1; 
    }
    
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!sockets[i].in_use) {
            memset(&sockets[i], 0, sizeof(socket_t));
            sockets[i].in_use = 1;
            sockets[i].type = type;
            sockets[i].domain = domain;
            
            if (type == SOCK_STREAM) {
                sockets[i].protocol = IP_PROTO_TCP;
            } else if (type == SOCK_DGRAM) {
                sockets[i].protocol = IP_PROTO_UDP;
            }
            
            return i;
        }
    }
    return -1;  
}

int socket_bind(int sockfd, const sockaddr_t *addr, uint32_t addrlen)
{
    (void)addrlen;
    
    socket_t *sock = get_socket(sockfd);
    if (!sock) return -1;
    
    const sockaddr_in_t *sin = (const sockaddr_in_t *)addr;
    sock->local_ip = ntohl(sin->sin_addr);
    sock->local_port = ntohs(sin->sin_port);
    sock->bound = 1;
    
    return 0;
}

int socket_listen(int sockfd, int backlog)
{
    (void)backlog;
    
    socket_t *sock = get_socket(sockfd);
    if (!sock || sock->type != SOCK_STREAM) return -1;
    
    sock->conn = tcp_listen(sock->local_port);
    if (!sock->conn) return -1;
    
    sock->listening = 1;
    return 0;
}

int socket_accept(int sockfd, sockaddr_t *addr, uint32_t *addrlen)
{
    (void)addr;
    (void)addrlen;
    
    socket_t *sock = get_socket(sockfd);
    if (!sock || !sock->listening) return -1;
    
    tcp_conn_t *conn = tcp_accept((tcp_conn_t *)sock->conn);
    if (!conn) return -1;
    
    int newfd = socket_create(AF_INET, SOCK_STREAM, 0);
    if (newfd < 0) return -1;
    
    socket_t *newsock = get_socket(newfd);
    newsock->conn = conn;
    newsock->connected = 1;
    newsock->remote_ip = conn->remote_ip;
    newsock->remote_port = conn->remote_port;
    
    return newfd;
}

int socket_connect(int sockfd, const sockaddr_t *addr, uint32_t addrlen)
{
    (void)addrlen;
    
    socket_t *sock = get_socket(sockfd);
    if (!sock) return -1;
    
    const sockaddr_in_t *sin = (const sockaddr_in_t *)addr;
    sock->remote_ip = ntohl(sin->sin_addr);
    sock->remote_port = ntohs(sin->sin_port);
    
    if (sock->type == SOCK_STREAM) {
        sock->conn = tcp_connect(sock->remote_ip, sock->remote_port);
        if (!sock->conn) return -1;
    }
    
    sock->connected = 1;
    return 0;
}

int socket_send(int sockfd, const void *buf, uint32_t len, int flags)
{
    (void)flags;
    
    socket_t *sock = get_socket(sockfd);
    if (!sock || !sock->connected) return -1;
    
    if (sock->type == SOCK_STREAM) {
        return tcp_send((tcp_conn_t *)sock->conn, buf, len);
    } else if (sock->type == SOCK_DGRAM) {
        return udp_send(sock->remote_ip, sock->local_port, sock->remote_port, buf, len);
    }
    
    return -1;
}

int socket_recv(int sockfd, void *buf, uint32_t len, int flags)
{
    (void)flags;
    
    socket_t *sock = get_socket(sockfd);
    if (!sock) return -1;
    
    if (sock->type == SOCK_STREAM) {
        return tcp_recv((tcp_conn_t *)sock->conn, buf, len);
    }
    
    return -1;
}

int socket_sendto(int sockfd, const void *buf, uint32_t len, int flags,
                  const sockaddr_t *dest_addr, uint32_t addrlen)
{
    (void)flags;
    (void)addrlen;
    
    socket_t *sock = get_socket(sockfd);
    if (!sock || sock->type != SOCK_DGRAM) return -1;
    
    const sockaddr_in_t *sin = (const sockaddr_in_t *)dest_addr;
    uint32_t dest_ip = ntohl(sin->sin_addr);
    uint16_t dest_port = ntohs(sin->sin_port);
    
    return udp_send(dest_ip, sock->local_port, dest_port, buf, len);
}

int socket_recvfrom(int sockfd, void *buf, uint32_t len, int flags,
                    sockaddr_t *src_addr, uint32_t *addrlen)
{
    (void)flags;
    (void)src_addr;
    (void)addrlen;
    (void)buf;
    (void)len;
    
    socket_t *sock = get_socket(sockfd);
    if (!sock || sock->type != SOCK_DGRAM) return -1;
    
    /* TODO: Implement UDP receive queue */
    return -1;
}

int socket_close(int sockfd)
{
    socket_t *sock = get_socket(sockfd);
    if (!sock) return -1;
    
    if (sock->type == SOCK_STREAM && sock->conn) {
        tcp_close((tcp_conn_t *)sock->conn);
    }
    
    sock->in_use = 0;
    return 0;
}

int socket_shutdown(int sockfd, int how)
{
    socket_t *sock = get_socket(sockfd);
    if (!sock) return -1;
    
    if (sock->type == SOCK_STREAM && sock->conn) {
        if (how == 1 || how == 2) {
            tcp_close((tcp_conn_t *)sock->conn);
        }
    }
    
    return 0;
}

int socket_getpeername(int sockfd, sockaddr_t *addr, uint32_t *addrlen)
{
    socket_t *sock = get_socket(sockfd);
    if (!sock || !sock->connected) return -1;
    
    sockaddr_in_t *sin = (sockaddr_in_t *)addr;
    sin->sin_family = AF_INET;
    sin->sin_port = htons(sock->remote_port);
    sin->sin_addr = htonl(sock->remote_ip);
    
    if (addrlen) *addrlen = sizeof(sockaddr_in_t);
    return 0;
}

int socket_getsockname(int sockfd, sockaddr_t *addr, uint32_t *addrlen)
{
    socket_t *sock = get_socket(sockfd);
    if (!sock) return -1;
    
    sockaddr_in_t *sin = (sockaddr_in_t *)addr;
    sin->sin_family = AF_INET;
    sin->sin_port = htons(sock->local_port);
    sin->sin_addr = htonl(sock->local_ip);
    
    if (addrlen) *addrlen = sizeof(sockaddr_in_t);
    return 0;
}

int socket_setsockopt(int sockfd, int level, int optname, const void *optval, uint32_t optlen)
{
    (void)level;
    (void)optval;
    (void)optlen;
    
    socket_t *sock = get_socket(sockfd);
    if (!sock) return -1;
    
    switch (optname) {
        case 2:
            return 0;
        case 9:
            return 0;
        case 1:
            return 0;
        default:
            return 0; 
    }
}

int socket_getsockopt(int sockfd, int level, int optname, void *optval, uint32_t *optlen)
{
    (void)level;
    
    socket_t *sock = get_socket(sockfd);
    if (!sock) return -1;
    
    /* Return default values */
    if (optval && optlen && *optlen >= 4) {
        *(int *)optval = 0;
        *optlen = 4;
    }
    
    (void)optname;
    return 0;
}

int socket_select(int nfds, uint32_t *readfds, uint32_t *writefds, uint32_t *exceptfds, uint32_t timeout_ms)
{
    (void)exceptfds;
    (void)timeout_ms;
    
    int ready = 0;
    
    for (int i = 0; i < nfds && i < MAX_SOCKETS; i++) {
        socket_t *sock = get_socket(i);
        if (!sock) continue;
        
        if (readfds && (*readfds & (1 << i))) {
            if (sock->listening) {
                ready++;
            }
        }
        
        /* Check write readiness - sockets are always writable for now */
        if (writefds && (*writefds & (1 << i))) {
            if (sock->connected || sock->type == SOCK_DGRAM) {
                ready++;
            }
        }
    }
    
    return ready;
}

int socket_get_info(int index, int *type, uint32_t *local_ip, uint16_t *local_port,
                    uint32_t *remote_ip, uint16_t *remote_port, int *state)
{
    if (index < 0 || index >= MAX_SOCKETS) return -1;
    if (!sockets[index].in_use) return -1;
    
    socket_t *sock = &sockets[index];
    
    *type = sock->type;
    *local_ip = sock->local_ip;
    *local_port = sock->local_port;
    *remote_ip = sock->remote_ip;
    *remote_port = sock->remote_port;
    
    if (sock->listening) *state = 1; 
    else if (sock->connected) *state = 2;  
    else *state = 0;  
    
    return 0;
}

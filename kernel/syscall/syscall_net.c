/* Network/Socket Syscalls
 * socket, bind, listen, accept, connect, send, recv, etc.
 */

#include <kernel/kernel.h>
#include <syscall/syscall_internal.h>
#include <net/socket.h>

int32_t sys_socket(uint32_t domain, uint32_t type, uint32_t protocol, uint32_t arg3, uint32_t arg4)
{
    (void)arg3; (void)arg4;
    extern int socket_create(int domain, int type, int protocol);
    return socket_create((int)domain, (int)type, (int)protocol);
}

int32_t sys_bind(uint32_t sockfd, uint32_t addr, uint32_t addrlen, uint32_t arg3, uint32_t arg4)
{
    (void)arg3; (void)arg4;
    extern int socket_bind(int sockfd, const struct sockaddr *addr, uint32_t addrlen);
    return socket_bind((int)sockfd, (const struct sockaddr *)addr, addrlen);
}

int32_t sys_listen(uint32_t sockfd, uint32_t backlog, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg2; (void)arg3; (void)arg4;
    extern int socket_listen(int sockfd, int backlog);
    return socket_listen((int)sockfd, (int)backlog);
}

int32_t sys_accept(uint32_t sockfd, uint32_t addr, uint32_t addrlen, uint32_t arg3, uint32_t arg4)
{
    (void)arg3; (void)arg4;
    extern int socket_accept(int sockfd, struct sockaddr *addr, uint32_t *addrlen);
    return socket_accept((int)sockfd, (struct sockaddr *)addr, (uint32_t *)addrlen);
}

int32_t sys_connect(uint32_t sockfd, uint32_t addr, uint32_t addrlen, uint32_t arg3, uint32_t arg4)
{
    (void)arg3; (void)arg4;
    extern int socket_connect(int sockfd, const struct sockaddr *addr, uint32_t addrlen);
    return socket_connect((int)sockfd, (const struct sockaddr *)addr, addrlen);
}

int32_t sys_send(uint32_t sockfd, uint32_t buf, uint32_t len, uint32_t flags, uint32_t arg4)
{
    (void)arg4;
    extern int socket_send(int sockfd, const void *buf, uint32_t len, int flags);
    return socket_send((int)sockfd, (const void *)buf, len, (int)flags);
}

int32_t sys_recv(uint32_t sockfd, uint32_t buf, uint32_t len, uint32_t flags, uint32_t arg4)
{
    (void)arg4;
    extern int socket_recv(int sockfd, void *buf, uint32_t len, int flags);
    return socket_recv((int)sockfd, (void *)buf, len, (int)flags);
}

int32_t sys_closesock(uint32_t sockfd, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    extern int socket_close(int sockfd);
    return socket_close((int)sockfd);
}

int32_t sys_shutdown(uint32_t sockfd, uint32_t how, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg2; (void)arg3; (void)arg4;
    extern int socket_shutdown(int sockfd, int how);
    return socket_shutdown((int)sockfd, (int)how);
}

int32_t sys_getsockname(uint32_t sockfd, uint32_t addr, uint32_t addrlen, uint32_t arg3, uint32_t arg4)
{
    (void)arg3; (void)arg4;
    extern int socket_getsockname(int sockfd, struct sockaddr *addr, uint32_t *addrlen);
    return socket_getsockname((int)sockfd, (struct sockaddr *)addr, (uint32_t *)addrlen);
}

int32_t sys_getpeername(uint32_t sockfd, uint32_t addr, uint32_t addrlen, uint32_t arg3, uint32_t arg4)
{
    (void)arg3; (void)arg4;
    extern int socket_getpeername(int sockfd, struct sockaddr *addr, uint32_t *addrlen);
    return socket_getpeername((int)sockfd, (struct sockaddr *)addr, (uint32_t *)addrlen);
}

int32_t sys_setsockopt(uint32_t sockfd, uint32_t level, uint32_t optname, uint32_t optval, uint32_t optlen)
{
    extern int socket_setsockopt(int sockfd, int level, int optname, const void *optval, uint32_t optlen);
    return socket_setsockopt((int)sockfd, (int)level, (int)optname, (const void *)optval, optlen);
}

int32_t sys_getsockopt(uint32_t sockfd, uint32_t level, uint32_t optname, uint32_t optval, uint32_t optlen)
{
    extern int socket_getsockopt(int sockfd, int level, int optname, void *optval, uint32_t *optlen);
    return socket_getsockopt((int)sockfd, (int)level, (int)optname, (void *)optval, (uint32_t *)optlen);
}

int32_t sys_select(uint32_t nfds, uint32_t readfds, uint32_t writefds, uint32_t exceptfds, uint32_t timeout)
{
    extern int socket_select(int nfds, uint32_t *readfds, uint32_t *writefds, uint32_t *exceptfds, uint32_t timeout_ms);
    return socket_select((int)nfds, (uint32_t *)readfds, (uint32_t *)writefds, (uint32_t *)exceptfds, timeout);
}

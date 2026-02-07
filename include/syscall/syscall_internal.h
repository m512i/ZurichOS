#ifndef _SYSCALL_INTERNAL_H
#define _SYSCALL_INTERNAL_H

#include <stdint.h>
#include <kernel/process.h>

#define MAX_FDS MAX_FDS_PER_PROC
#define FD_STDIN  0
#define FD_STDOUT 1
#define FD_STDERR 2

typedef int32_t (*syscall_handler_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

fd_entry_t *get_fd_table(void);
int alloc_fd(void);
void free_fd(int fd);
int validate_user_ptr(uint32_t ptr, uint32_t size);
int validate_user_string(uint32_t ptr, uint32_t max_len);

int32_t sys_exit(uint32_t status, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_read(uint32_t fd, uint32_t buf, uint32_t count, uint32_t arg3, uint32_t arg4);
int32_t sys_write(uint32_t fd, uint32_t buf, uint32_t count, uint32_t arg3, uint32_t arg4);
int32_t sys_open(uint32_t path, uint32_t flags, uint32_t mode, uint32_t arg3, uint32_t arg4);
int32_t sys_close(uint32_t fd, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_lseek(uint32_t fd, uint32_t offset, uint32_t whence, uint32_t arg3, uint32_t arg4);
int32_t sys_stat(uint32_t path, uint32_t stat_buf, uint32_t arg2, uint32_t arg3, uint32_t arg4);

int32_t sys_getpid(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_fork(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_exec(uint32_t path, uint32_t argv, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_waitpid(uint32_t pid, uint32_t status, uint32_t options, uint32_t arg3, uint32_t arg4);
int32_t sys_kill(uint32_t pid, uint32_t sig, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_getppid(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_setpgid(uint32_t pid, uint32_t pgid, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_getpgid(uint32_t pid, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_sigaction(uint32_t sig, uint32_t handler, uint32_t oldact, uint32_t arg3, uint32_t arg4);
int32_t sys_sigprocmask(uint32_t how, uint32_t set, uint32_t oldset, uint32_t arg3, uint32_t arg4);
int32_t sys_pipe(uint32_t pipefd, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_shmget(uint32_t key, uint32_t size, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_shmat(uint32_t shmid, uint32_t vaddr, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_shmdt(uint32_t addr, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_msgget(uint32_t key, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_msgsnd(uint32_t msqid, uint32_t msgp, uint32_t msgsz, uint32_t mtype, uint32_t arg4);
int32_t sys_msgrcv(uint32_t msqid, uint32_t msgp, uint32_t msgsz, uint32_t mtype, uint32_t arg4);
int32_t sys_mmap_handler(uint32_t addr, uint32_t length, uint32_t prot, uint32_t flags, uint32_t fd);
int32_t sys_munmap_handler(uint32_t addr, uint32_t length, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_mprotect_handler(uint32_t addr, uint32_t length, uint32_t prot, uint32_t arg3, uint32_t arg4);
int32_t sys_brk_handler(uint32_t addr, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);

int32_t sys_socket(uint32_t domain, uint32_t type, uint32_t protocol, uint32_t arg3, uint32_t arg4);
int32_t sys_bind(uint32_t sockfd, uint32_t addr, uint32_t addrlen, uint32_t arg3, uint32_t arg4);
int32_t sys_listen(uint32_t sockfd, uint32_t backlog, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_accept(uint32_t sockfd, uint32_t addr, uint32_t addrlen, uint32_t arg3, uint32_t arg4);
int32_t sys_connect(uint32_t sockfd, uint32_t addr, uint32_t addrlen, uint32_t arg3, uint32_t arg4);
int32_t sys_send(uint32_t sockfd, uint32_t buf, uint32_t len, uint32_t flags, uint32_t arg4);
int32_t sys_recv(uint32_t sockfd, uint32_t buf, uint32_t len, uint32_t flags, uint32_t arg4);
int32_t sys_closesock(uint32_t sockfd, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_shutdown(uint32_t sockfd, uint32_t how, uint32_t arg2, uint32_t arg3, uint32_t arg4);
int32_t sys_getsockname(uint32_t sockfd, uint32_t addr, uint32_t addrlen, uint32_t arg3, uint32_t arg4);
int32_t sys_getpeername(uint32_t sockfd, uint32_t addr, uint32_t addrlen, uint32_t arg3, uint32_t arg4);
int32_t sys_setsockopt(uint32_t sockfd, uint32_t level, uint32_t optname, uint32_t optval, uint32_t optlen);
int32_t sys_getsockopt(uint32_t sockfd, uint32_t level, uint32_t optname, uint32_t optval, uint32_t optlen);
int32_t sys_select(uint32_t nfds, uint32_t readfds, uint32_t writefds, uint32_t exceptfds, uint32_t timeout);

#endif

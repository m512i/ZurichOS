/* Inter-Process Communication
 * Pipes, shared memory, message queues
 */

#ifndef _KERNEL_IPC_H
#define _KERNEL_IPC_H

#include <stdint.h>

#define PIPE_BUF_SIZE   4096
#define MAX_PIPES       32

typedef struct pipe {
    uint8_t buffer[PIPE_BUF_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t count;
    uint32_t readers;
    uint32_t writers;
    int read_fd;
    int write_fd;
    int in_use;
} pipe_t;

#define MAX_SHM_REGIONS     16
#define SHM_MAX_SIZE        (1024 * 1024)  
typedef struct shm_region {
    uint32_t key;
    uint32_t size;
    uint32_t phys_addr;
    uint32_t ref_count;
    int in_use;
} shm_region_t;

#define MAX_MSG_QUEUES      16
#define MAX_MSG_SIZE        256
#define MAX_MSGS_PER_QUEUE  32
typedef struct msg {
    long mtype;
    uint8_t mtext[MAX_MSG_SIZE];
    uint32_t msize;
} msg_t;

typedef struct msg_queue {
    uint32_t key;
    msg_t messages[MAX_MSGS_PER_QUEUE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    int in_use;
} msg_queue_t;

int pipe_create(int pipefd[2]);
int pipe_read(int fd, void *buf, uint32_t count);
int pipe_write(int fd, const void *buf, uint32_t count);
int pipe_close(int fd);
int pipe_is_pipe(int fd);

int shm_create(uint32_t key, uint32_t size);
void *shm_attach(int shmid, uint32_t vaddr);
int shm_detach(void *addr);
int shm_destroy(int shmid);

int msgq_create(uint32_t key);
int msgq_send(int msqid, const void *msgp, uint32_t msgsz, long mtype);
int msgq_receive(int msqid, void *msgp, uint32_t msgsz, long mtype);
int msgq_destroy(int msqid);

int fifo_create(const char *path);
int fifo_open(const char *path, int flags);

void ipc_init(void);

#endif

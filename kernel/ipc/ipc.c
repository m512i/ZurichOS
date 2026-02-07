/* Inter-Process Communication
 * Pipes, shared memory, message queues
 */

#include <kernel/ipc.h>
#include <kernel/process.h>
#include <kernel/signal.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <drivers/serial.h>
#include <string.h>

static pipe_t pipes[MAX_PIPES];
static shm_region_t shm_regions[MAX_SHM_REGIONS];
static msg_queue_t msg_queues[MAX_MSG_QUEUES];

void ipc_init(void)
{
    memset(pipes, 0, sizeof(pipes));
    memset(shm_regions, 0, sizeof(shm_regions));
    memset(msg_queues, 0, sizeof(msg_queues));
    serial_puts("[IPC] Initialized\n");
}

/* Helper function for future use - currently pipe operations inline the logic */
__attribute__((unused))
static pipe_t *get_pipe_by_fd(int fd)
{
    process_t *proc = process_current();
    if (!proc || fd < 0 || fd >= MAX_FDS_PER_PROC) return NULL;
    if (!proc->fd_table[fd].in_use) return NULL;
    
    int pipe_id = proc->fd_table[fd].pipe_id;
    if (pipe_id < 0 || pipe_id >= MAX_PIPES) return NULL;
    if (!pipes[pipe_id].in_use) return NULL;
    
    return &pipes[pipe_id];
}

int pipe_create(int pipefd[2])
{
    int pipe_id = -1;
    for (int i = 0; i < MAX_PIPES; i++) {
        if (!pipes[i].in_use) {
            pipe_id = i;
            break;
        }
    }
    
    if (pipe_id < 0) {
        return -24; 
    }
    
    process_t *proc = process_current();
    if (!proc) return -1;
    
    int read_fd = -1, write_fd = -1;
    for (int i = 3; i < MAX_FDS_PER_PROC && (read_fd < 0 || write_fd < 0); i++) {
        if (!proc->fd_table[i].in_use) {
            if (read_fd < 0) read_fd = i;
            else write_fd = i;
        }
    }
    
    if (read_fd < 0 || write_fd < 0) {
        return -24;  
    }
    
    pipe_t *p = &pipes[pipe_id];
    memset(p, 0, sizeof(pipe_t));
    p->in_use = 1;
    p->readers = 1;
    p->writers = 1;
    p->read_fd = read_fd;
    p->write_fd = write_fd;
    
    proc->fd_table[read_fd].in_use = 1;
    proc->fd_table[read_fd].pipe_id = pipe_id;
    proc->fd_table[read_fd].flags = 0;  
    proc->fd_table[read_fd].node = NULL;
    
    proc->fd_table[write_fd].in_use = 1;
    proc->fd_table[write_fd].pipe_id = pipe_id;
    proc->fd_table[write_fd].flags = 1; 
    proc->fd_table[write_fd].node = NULL;
    
    pipefd[0] = read_fd;
    pipefd[1] = write_fd;
    
    serial_puts("[PIPE] Created pipe ");
    char buf[4];
    buf[0] = '0' + pipe_id;
    buf[1] = '\0';
    serial_puts(buf);
    serial_puts("\n");
    
    return 0;
}

int pipe_read(int fd, void *buf, uint32_t count)
{
    process_t *proc = process_current();
    if (!proc || fd < 0 || fd >= MAX_FDS_PER_PROC) return -9;
    if (!proc->fd_table[fd].in_use) return -9;
    
    int pipe_id = proc->fd_table[fd].pipe_id;
    if (pipe_id < 0 || pipe_id >= MAX_PIPES) return -9;
    
    pipe_t *p = &pipes[pipe_id];
    if (!p->in_use) return -9;
    
    if (proc->fd_table[fd].flags != 0) {
        return -9;  
    }
    
    uint8_t *buffer = (uint8_t *)buf;
    uint32_t bytes_read = 0;
    
    while (bytes_read < count) {
        if (p->count > 0) {
            buffer[bytes_read++] = p->buffer[p->read_pos];
            p->read_pos = (p->read_pos + 1) % PIPE_BUF_SIZE;
            p->count--;
        } else if (p->writers == 0) {
            break;
        } else if (bytes_read > 0) {
            break;
        } else {
            break;
        }
    }
    
    return (int)bytes_read;
}

int pipe_write(int fd, const void *buf, uint32_t count)
{
    process_t *proc = process_current();
    if (!proc || fd < 0 || fd >= MAX_FDS_PER_PROC) return -9;
    if (!proc->fd_table[fd].in_use) return -9;
    
    int pipe_id = proc->fd_table[fd].pipe_id;
    if (pipe_id < 0 || pipe_id >= MAX_PIPES) return -9;
    
    pipe_t *p = &pipes[pipe_id];
    if (!p->in_use) return -9;
    
    if (proc->fd_table[fd].flags != 1) {
        return -9;  
    }
    
    if (p->readers == 0) {
        process_signal(proc->pid, SIGPIPE);
        return -32;  
    }
    
    const uint8_t *buffer = (const uint8_t *)buf;
    uint32_t bytes_written = 0;
    
    while (bytes_written < count) {
        if (p->count < PIPE_BUF_SIZE) {
            p->buffer[p->write_pos] = buffer[bytes_written++];
            p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
            p->count++;
        } else {
            break;
        }
    }
    
    return (int)bytes_written;
}

int pipe_close(int fd)
{
    process_t *proc = process_current();
    if (!proc || fd < 0 || fd >= MAX_FDS_PER_PROC) return -9;
    if (!proc->fd_table[fd].in_use) return -9;
    
    int pipe_id = proc->fd_table[fd].pipe_id;
    if (pipe_id < 0 || pipe_id >= MAX_PIPES) return 0;
    
    pipe_t *p = &pipes[pipe_id];
    if (!p->in_use) return 0;
    
    if (proc->fd_table[fd].flags == 0) {
        if (p->readers > 0) p->readers--;
    } else {
        if (p->writers > 0) p->writers--;
    }
    
    if (p->readers == 0 && p->writers == 0) {
        p->in_use = 0;
        serial_puts("[PIPE] Destroyed pipe\n");
    }
    
    proc->fd_table[fd].in_use = 0;
    proc->fd_table[fd].pipe_id = -1;
    
    return 0;
}

int pipe_is_pipe(int fd)
{
    process_t *proc = process_current();
    if (!proc || fd < 0 || fd >= MAX_FDS_PER_PROC) return 0;
    if (!proc->fd_table[fd].in_use) return 0;
    
    int pipe_id = proc->fd_table[fd].pipe_id;
    return (pipe_id >= 0 && pipe_id < MAX_PIPES && pipes[pipe_id].in_use);
}

int shm_create(uint32_t key, uint32_t size)
{
    if (size == 0 || size > SHM_MAX_SIZE) {
        return -22;  
    }
    
    for (int i = 0; i < MAX_SHM_REGIONS; i++) {
        if (shm_regions[i].in_use && shm_regions[i].key == key) {
            return i;  
        }
    }
    
    int shmid = -1;
    for (int i = 0; i < MAX_SHM_REGIONS; i++) {
        if (!shm_regions[i].in_use) {
            shmid = i;
            break;
        }
    }
    
    if (shmid < 0) {
        return -28;  
    }
    
    uint32_t pages = (size + 0xFFF) / 0x1000;
    uint32_t phys = pmm_alloc_frame();
    if (phys == 0) {
        return -12;  
    }
    for (uint32_t i = 1; i < pages; i++) {
        uint32_t next = pmm_alloc_frame();
        if (next == 0) {
            for (uint32_t j = 0; j < i; j++) {
                pmm_free_frame(phys + j * 0x1000);
            }
            return -12;
        }
    }
    
    shm_region_t *shm = &shm_regions[shmid];
    shm->key = key;
    shm->size = size;
    shm->phys_addr = phys;
    shm->ref_count = 0;
    shm->in_use = 1;
    
    serial_puts("[SHM] Created shared memory region\n");
    return shmid;
}

void *shm_attach(int shmid, uint32_t vaddr)
{
    if (shmid < 0 || shmid >= MAX_SHM_REGIONS) {
        return (void *)-1;
    }
    
    shm_region_t *shm = &shm_regions[shmid];
    if (!shm->in_use) {
        return (void *)-1;
    }
    
    uint32_t pages = (shm->size + 0xFFF) / 0x1000;
    for (uint32_t i = 0; i < pages; i++) {
        vmm_map_page(vaddr + i * 0x1000, 
                     shm->phys_addr + i * 0x1000,
                     PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }
    
    shm->ref_count++;
    
    serial_puts("[SHM] Attached shared memory\n");
    return (void *)vaddr;
}

int shm_detach(void *addr)
{
    uint32_t vaddr = (uint32_t)addr;
    
    for (int i = 0; i < MAX_SHM_REGIONS; i++) {
        if (!shm_regions[i].in_use) continue;
        
        uint32_t pages = (shm_regions[i].size + 0xFFF) / 0x1000;
        for (uint32_t j = 0; j < pages; j++) {
            vmm_unmap_page(vaddr + j * 0x1000);
        }
        
        if (shm_regions[i].ref_count > 0) {
            shm_regions[i].ref_count--;
        }
        
        serial_puts("[SHM] Detached shared memory\n");
        return 0;
    }
    
    return -22; 
}

int shm_destroy(int shmid)
{
    if (shmid < 0 || shmid >= MAX_SHM_REGIONS) {
        return -22;
    }
    
    shm_region_t *shm = &shm_regions[shmid];
    if (!shm->in_use) {
        return -22;
    }
    
    if (shm->ref_count > 0) {
        return -16;  
    }
    
    uint32_t pages = (shm->size + 0xFFF) / 0x1000;
    for (uint32_t i = 0; i < pages; i++) {
        pmm_free_frame(shm->phys_addr + i * 0x1000);
    }
    
    shm->in_use = 0;
    
    serial_puts("[SHM] Destroyed shared memory region\n");
    return 0;
}

int msgq_create(uint32_t key)
{
    for (int i = 0; i < MAX_MSG_QUEUES; i++) {
        if (msg_queues[i].in_use && msg_queues[i].key == key) {
            return i;
        }
    }
    
    int msqid = -1;
    for (int i = 0; i < MAX_MSG_QUEUES; i++) {
        if (!msg_queues[i].in_use) {
            msqid = i;
            break;
        }
    }
    
    if (msqid < 0) {
        return -28;  
    }
    
    msg_queue_t *mq = &msg_queues[msqid];
    memset(mq, 0, sizeof(msg_queue_t));
    mq->key = key;
    mq->in_use = 1;
    
    serial_puts("[MSGQ] Created message queue\n");
    return msqid;
}

int msgq_send(int msqid, const void *msgp, uint32_t msgsz, long mtype)
{
    if (msqid < 0 || msqid >= MAX_MSG_QUEUES) {
        return -22;
    }
    
    if (msgsz > MAX_MSG_SIZE) {
        return -22;
    }
    
    msg_queue_t *mq = &msg_queues[msqid];
    if (!mq->in_use) {
        return -22;
    }
    
    if (mq->count >= MAX_MSGS_PER_QUEUE) {
        return -11;  
    }
    
    msg_t *msg = &mq->messages[mq->tail];
    msg->mtype = mtype;
    msg->msize = msgsz;
    memcpy(msg->mtext, msgp, msgsz);
    
    mq->tail = (mq->tail + 1) % MAX_MSGS_PER_QUEUE;
    mq->count++;
    
    return 0;
}

int msgq_receive(int msqid, void *msgp, uint32_t msgsz, long mtype)
{
    if (msqid < 0 || msqid >= MAX_MSG_QUEUES) {
        return -22;
    }
    
    msg_queue_t *mq = &msg_queues[msqid];
    if (!mq->in_use) {
        return -22;
    }
    
    if (mq->count == 0) {
        return -11; 
    }
    
    for (uint32_t i = 0; i < mq->count; i++) {
        uint32_t idx = (mq->head + i) % MAX_MSGS_PER_QUEUE;
        msg_t *msg = &mq->messages[idx];
        
        if (mtype == 0 || msg->mtype == mtype) {
            uint32_t copy_size = msg->msize < msgsz ? msg->msize : msgsz;
            memcpy(msgp, msg->mtext, copy_size);
            
            for (uint32_t j = i; j < mq->count - 1; j++) {
                uint32_t src = (mq->head + j + 1) % MAX_MSGS_PER_QUEUE;
                uint32_t dst = (mq->head + j) % MAX_MSGS_PER_QUEUE;
                mq->messages[dst] = mq->messages[src];
            }
            
            mq->count--;
            if (mq->tail > 0) mq->tail--;
            else mq->tail = MAX_MSGS_PER_QUEUE - 1;
            
            return (int)copy_size;
        }
    }
    
    return -42;  
}

int msgq_destroy(int msqid)
{
    if (msqid < 0 || msqid >= MAX_MSG_QUEUES) {
        return -22;
    }
    
    msg_queue_t *mq = &msg_queues[msqid];
    if (!mq->in_use) {
        return -22;
    }
    
    mq->in_use = 0;
    
    serial_puts("[MSGQ] Destroyed message queue\n");
    return 0;
}

int fifo_create(const char *path)
{
    (void)path;
    serial_puts("[FIFO] FIFO creation not fully implemented\n");
    return -38;  
}

int fifo_open(const char *path, int flags)
{
    (void)path;
    (void)flags;
    serial_puts("[FIFO] FIFO open not fully implemented\n");
    return -38; 
}

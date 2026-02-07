#ifndef _SYSCALL_SYSCALL_H
#define _SYSCALL_SYSCALL_H

#include <stdint.h>

#define SYSCALL_EXIT    0
#define SYSCALL_READ    1
#define SYSCALL_WRITE   2
#define SYSCALL_OPEN    3
#define SYSCALL_CLOSE   4
#define SYSCALL_GETPID  5

void syscall_init(void);
void syscall_set_shell_stack(uint32_t base, uint32_t top);

#endif

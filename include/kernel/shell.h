#ifndef _KERNEL_SHELL_H
#define _KERNEL_SHELL_H

#include <stdint.h>

#define SHELL_BUFFER_SIZE   256
#define SHELL_MAX_ARGS      16
#define SHELL_PROMPT        "zurich> "

void shell_init(void);
void shell_run(void);
void shell_input(char c);

struct vfs_node;
struct vfs_node *shell_get_cwd(void);

#endif

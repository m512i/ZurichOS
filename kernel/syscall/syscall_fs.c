/* Filesystem Syscalls
 * read, write, open, close, lseek, stat
 */

#include <kernel/kernel.h>
#include <kernel/process.h>
#include <syscall/syscall_internal.h>
#include <drivers/vga.h>
#include <drivers/serial.h>
#include <drivers/keyboard.h>
#include <fs/vfs.h>
#include <string.h>

int32_t sys_read(uint32_t fd, uint32_t buf, uint32_t count, uint32_t arg3, uint32_t arg4)
{
    (void)arg3; (void)arg4;

    if (count == 0) return 0;

    if (!validate_user_ptr(buf, count)) {
        return -14;
    }

    if (fd == FD_STDIN) {
        char *buffer = (char *)buf;
        uint32_t bytes_read = 0;

        while (bytes_read < count) {
            key_event_t event;
            if (keyboard_get_event(&event)) {
                if (event.pressed && event.ascii != 0) {
                    char c = event.ascii;
                    buffer[bytes_read++] = c;
                    if (c == '\n') break;
                }
            } else {
                if (bytes_read > 0) break;
                __asm__ volatile("hlt");
            }
        }

        return (int32_t)bytes_read;
    }

    fd_entry_t *fd_table = get_fd_table();
    if (!fd_table) return -9;

    if (fd >= MAX_FDS || !fd_table[fd].in_use) {
        return -9;
    }

    vfs_node_t *node = fd_table[fd].node;
    if (!node) {
        return -9;
    }

    uint8_t *buffer = (uint8_t *)buf;
    int bytes_read = vfs_read(node, fd_table[fd].offset, count, buffer);

    if (bytes_read > 0) {
        fd_table[fd].offset += bytes_read;
    }

    return bytes_read;
}

int32_t sys_write(uint32_t fd, uint32_t buf, uint32_t count, uint32_t arg3, uint32_t arg4)
{
    (void)arg3; (void)arg4;

    if (count == 0) return 0;

    if (!validate_user_ptr(buf, count)) {
        return -14;
    }

    char *str = (char *)buf;

    if (fd == FD_STDOUT || fd == FD_STDERR) {
        for (uint32_t i = 0; i < count; i++) {
            vga_putchar(str[i]);
            serial_putc(str[i]);
        }
        return (int32_t)count;
    }

    fd_entry_t *fd_table = get_fd_table();
    if (!fd_table) return -9;

    if (fd >= MAX_FDS || !fd_table[fd].in_use) {
        return -9;
    }

    vfs_node_t *node = fd_table[fd].node;
    if (!node) {
        return -9;
    }
    
    uint8_t *buffer = (uint8_t *)buf;
    int bytes_written;

    if (fd_table[fd].flags & VFS_O_APPEND) {
        bytes_written = vfs_append(node, count, buffer);
    } else {
        bytes_written = vfs_write(node, fd_table[fd].offset, count, buffer);
    }

    if (bytes_written > 0) {
        fd_table[fd].offset += bytes_written;
    }

    return bytes_written;
}

int32_t sys_open(uint32_t path, uint32_t flags, uint32_t mode, uint32_t arg3, uint32_t arg4)
{
    (void)mode; (void)arg3; (void)arg4;

    if (!validate_user_string(path, VFS_MAX_PATH)) {
        return -14;
    }

    char *pathname = (char *)path;

    vfs_node_t *node = vfs_lookup(pathname);

    if (!node && (flags & VFS_O_CREAT)) {
        char path_copy[VFS_MAX_PATH];
        strncpy(path_copy, pathname, VFS_MAX_PATH - 1);
        path_copy[VFS_MAX_PATH - 1] = '\0';

        char *last_slash = NULL;
        for (char *p = path_copy; *p; p++) {
            if (*p == '/') last_slash = p;
        }

        if (last_slash) {
            *last_slash = '\0';
            char *filename = last_slash + 1;

            vfs_node_t *parent;
            if (last_slash == path_copy) {
                parent = vfs_get_root();
            } else {
                parent = vfs_lookup(path_copy);
            }

            if (parent && vfs_is_directory(parent)) {
                if (vfs_create(parent, filename, VFS_FILE) == 0) {
                    node = vfs_finddir(parent, filename);
                }
            }
        }
    }

    if (!node) {
        return -2;
    }

    if (vfs_is_directory(node)) {
        return -21;
    }

    int fd = alloc_fd();
    if (fd < 0) {
        return -24;
    }

    if (flags & VFS_O_TRUNC) {
        vfs_truncate(node);
    }

    fd_entry_t *fd_table = get_fd_table();
    if (!fd_table) return -9;

    fd_table[fd].node = node;
    fd_table[fd].flags = flags;
    fd_table[fd].offset = 0;

    if (flags & VFS_O_APPEND) {
        fd_table[fd].offset = node->length;
    }

    vfs_open(node, flags);

    return fd;
}

int32_t sys_close(uint32_t fd, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;

    if (fd < 3) {
        return 0;
    }

    fd_entry_t *fd_table = get_fd_table();
    if (!fd_table) return -9;

    if (fd >= MAX_FDS || !fd_table[fd].in_use) {
        return -9;
    }

    vfs_node_t *node = fd_table[fd].node;
    if (node) {
        vfs_close(node);
    }

    free_fd(fd);

    return 0;
}

int32_t sys_lseek(uint32_t fd, uint32_t offset, uint32_t whence, uint32_t arg3, uint32_t arg4)
{
    (void)arg3; (void)arg4;

    if (fd < 3) {
        return -29;
    }

    fd_entry_t *fd_table = get_fd_table();
    if (!fd_table) return -9;

    if (fd >= MAX_FDS || !fd_table[fd].in_use) {
        return -9;
    }

    vfs_node_t *node = fd_table[fd].node;
    if (!node) {
        return -9;
    }

    int32_t new_offset;

    switch (whence) {
        case VFS_SEEK_SET:
            new_offset = (int32_t)offset;
            break;
        case VFS_SEEK_CUR:
            new_offset = (int32_t)fd_table[fd].offset + (int32_t)offset;
            break;
        case VFS_SEEK_END:
            new_offset = (int32_t)node->length + (int32_t)offset;
            break;
        default:
            return -22;
    }

    if (new_offset < 0) {
        return -22;
    }

    fd_table[fd].offset = (uint32_t)new_offset;
    return new_offset;
}

int32_t sys_stat(uint32_t path, uint32_t stat_buf, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    (void)arg2; (void)arg3; (void)arg4;

    if (!validate_user_string(path, VFS_MAX_PATH)) {
        return -14;
    }

    if (!validate_user_ptr(stat_buf, 32)) {
        return -14;
    }

    char *pathname = (char *)path;
    vfs_node_t *node = vfs_lookup(pathname);

    if (!node) {
        return -2;
    }

    uint32_t *stat = (uint32_t *)stat_buf;
    stat[0] = node->inode;
    stat[1] = node->flags;
    stat[2] = node->length;
    stat[3] = node->uid;
    stat[4] = node->gid;
    stat[5] = node->permissions;
    stat[6] = node->mtime;
    stat[7] = node->ctime;

    return 0;
}

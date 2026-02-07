#ifndef _FS_VFS_H
#define _FS_VFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define VFS_MAX_PATH        256
#define VFS_MAX_NAME        64

#define VFS_FILE            0x01
#define VFS_DIRECTORY       0x02
#define VFS_CHARDEVICE      0x03
#define VFS_BLOCKDEVICE     0x04
#define VFS_PIPE            0x05
#define VFS_SYMLINK         0x06
#define VFS_MOUNTPOINT      0x08

#define VFS_O_RDONLY        0x0001
#define VFS_O_WRONLY        0x0002
#define VFS_O_RDWR          0x0003
#define VFS_O_APPEND        0x0008
#define VFS_O_CREAT         0x0100
#define VFS_O_TRUNC         0x0200
#define VFS_O_EXCL          0x0400

#define VFS_SEEK_SET        0
#define VFS_SEEK_CUR        1
#define VFS_SEEK_END        2

struct vfs_node;
struct dirent;

typedef int (*read_fn)(struct vfs_node *, uint32_t offset, uint32_t size, uint8_t *buffer);
typedef int (*write_fn)(struct vfs_node *, uint32_t offset, uint32_t size, uint8_t *buffer);
typedef int (*open_fn)(struct vfs_node *, uint32_t flags);
typedef int (*close_fn)(struct vfs_node *);
typedef struct dirent *(*readdir_fn)(struct vfs_node *, uint32_t index);
typedef struct vfs_node *(*finddir_fn)(struct vfs_node *, const char *name);
typedef int (*create_fn)(struct vfs_node *, const char *name, uint32_t type);
typedef int (*unlink_fn)(struct vfs_node *, const char *name);
typedef struct vfs_node {
    char name[VFS_MAX_NAME];    
    uint32_t flags;             
    uint32_t length;            
    uint32_t inode;             
    uint32_t uid;               
    uint32_t gid;               
    uint32_t permissions;       
    uint32_t ctime;                 uint32_t mtime;             
    uint32_t atime;             
    
    read_fn read;
    write_fn write;
    open_fn open;
    close_fn close;
    readdir_fn readdir;
    finddir_fn finddir;
    create_fn create;
    unlink_fn unlink;
    
    void *impl;                 
    struct vfs_node *parent;   
    struct vfs_node *ptr;       
} vfs_node_t;

typedef struct dirent {
    char name[VFS_MAX_NAME];
    uint32_t inode;
} dirent_t;

void vfs_init(void);

int vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
int vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
int vfs_append(vfs_node_t *node, uint32_t size, uint8_t *buffer);
int vfs_open(vfs_node_t *node, uint32_t flags);
int vfs_close(vfs_node_t *node);
int vfs_truncate(vfs_node_t *node);

dirent_t *vfs_readdir(vfs_node_t *node, uint32_t index);
vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name);
int vfs_create(vfs_node_t *dir, const char *name, uint32_t type);
int vfs_unlink(vfs_node_t *dir, const char *name);
int vfs_mkdir(const char *path);

vfs_node_t *vfs_lookup(const char *path);
vfs_node_t *vfs_get_root(void);
void vfs_set_root(vfs_node_t *root);

int vfs_mount(const char *path, vfs_node_t *fs_root);
int vfs_unmount(const char *path);

bool vfs_is_directory(vfs_node_t *node);

#endif 

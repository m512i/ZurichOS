#ifndef _FS_RAMFS_H
#define _FS_RAMFS_H

#include <fs/vfs.h>


#define RAMFS_MAX_FILES     128
#define RAMFS_MAX_FILE_SIZE (64 * 1024)

vfs_node_t *ramfs_init(void);
vfs_node_t *ramfs_create_file(vfs_node_t *parent, const char *name);
vfs_node_t *ramfs_create_dir(vfs_node_t *parent, const char *name);
int ramfs_write_file(vfs_node_t *node, const void *data, uint32_t size);

#endif 
/* Device Filesystem Header */

#ifndef _FS_DEVFS_H
#define _FS_DEVFS_H

#include <fs/vfs.h>
#include <stdint.h>

#define DEV_TYPE_CHAR   1
#define DEV_TYPE_BLOCK  2

vfs_node_t *devfs_init(vfs_node_t *parent);
int devfs_register(const char *name, int type, int major, int minor,
                   uint32_t (*read)(void *, uint32_t),
                   uint32_t (*write)(const void *, uint32_t));
int devfs_get_count(void);
void devfs_list(void);

#endif

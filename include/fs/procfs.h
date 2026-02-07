/* Process Filesystem Header */

#ifndef _FS_PROCFS_H
#define _FS_PROCFS_H

#include <fs/vfs.h>

vfs_node_t *procfs_init(vfs_node_t *parent);

#endif

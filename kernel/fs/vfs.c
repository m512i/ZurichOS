/* Virtual File System (VFS)
 * Provides a unified interface for filesystem operations
 */

#include <fs/vfs.h>
#include <string.h>

static vfs_node_t *vfs_root = NULL;

#define MAX_MOUNTS 8
typedef struct {
    char path[VFS_MAX_PATH];
    vfs_node_t *mount_point;    
    vfs_node_t *fs_root;       
} mount_entry_t;

static mount_entry_t mount_table[MAX_MOUNTS];
static int mount_count = 0;

void vfs_init(void)
{
    vfs_root = NULL;
    mount_count = 0;
    memset(mount_table, 0, sizeof(mount_table));
}

int vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    if (!node || !node->read) {
        return -1;
    }
    return node->read(node, offset, size, buffer);
}

int vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    if (!node || !node->write) {
        return -1;
    }
    return node->write(node, offset, size, buffer);
}

int vfs_append(vfs_node_t *node, uint32_t size, uint8_t *buffer)
{
    if (!node || !node->write) {
        return -1;
    }
    return node->write(node, node->length, size, buffer);
}

int vfs_truncate(vfs_node_t *node)
{
    if (!node) {
        return -1;
    }
    node->length = 0;
    return 0;
}

int vfs_open(vfs_node_t *node, uint32_t flags)
{
    if (!node) {
        return -1;
    }
    if (node->open) {
        return node->open(node, flags);
    }
    return 0;  
}

int vfs_close(vfs_node_t *node)
{
    if (!node) {
        return -1;
    }
    if (node->close) {
        return node->close(node);
    }
    return 0;
}

dirent_t *vfs_readdir(vfs_node_t *node, uint32_t index)
{
    if (!node || !(node->flags & VFS_DIRECTORY) || !node->readdir) {
        return NULL;
    }
    return node->readdir(node, index);
}

vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name)
{
    if (!node || !(node->flags & VFS_DIRECTORY) || !node->finddir) {
        return NULL;
    }
    vfs_node_t *result = node->finddir(node, name);
    
    if (result && (result->flags & VFS_MOUNTPOINT) && result->ptr) {
        result = result->ptr;
    }
    
    return result;
}

int vfs_create(vfs_node_t *dir, const char *name, uint32_t type)
{
    if (!dir || !(dir->flags & VFS_DIRECTORY) || !dir->create) {
        return -1;
    }
    return dir->create(dir, name, type);
}

int vfs_unlink(vfs_node_t *dir, const char *name)
{
    if (!dir || !(dir->flags & VFS_DIRECTORY) || !dir->unlink) {
        return -1;
    }
    return dir->unlink(dir, name);
}

vfs_node_t *vfs_get_root(void)
{
    return vfs_root;
}

void vfs_set_root(vfs_node_t *root)
{
    vfs_root = root;
}

bool vfs_is_directory(vfs_node_t *node)
{
    return node && (node->flags & VFS_DIRECTORY);
}

vfs_node_t *vfs_lookup(const char *path)
{
    if (!path || !vfs_root) {
        return NULL;
    }
    
    if (path[0] == '/' && path[1] == '\0') {
        return vfs_root;
    }
    
    if (*path == '/') {
        path++;
    }
    
    vfs_node_t *current = vfs_root;
    char component[VFS_MAX_NAME];
    
    while (*path && current) {
        int i = 0;
        while (*path && *path != '/' && i < VFS_MAX_NAME - 1) {
            component[i++] = *path++;
        }
        component[i] = '\0';
        
        while (*path == '/') {
            path++;
        }
        
        if (strcmp(component, ".") == 0) {
            continue;
        }
        if (strcmp(component, "..") == 0) {
            if (current->parent) {
                current = current->parent;
            }
            continue;
        }
        
        if (!(current->flags & VFS_DIRECTORY)) {
            return NULL;  
        }
        
        current = vfs_finddir(current, component);
        
        if (current && (current->flags & VFS_MOUNTPOINT) && current->ptr) {
            current = current->ptr;
        }
    }
    
    return current;
}

int vfs_mkdir(const char *path)
{
    if (!path || !vfs_root) {
        return -1;
    }
    
    char parent_path[VFS_MAX_PATH];
    char name[VFS_MAX_NAME];
    
    strncpy(parent_path, path, VFS_MAX_PATH - 1);
    parent_path[VFS_MAX_PATH - 1] = '\0';
    
    char *last_slash = NULL;
    for (char *p = parent_path; *p; p++) {
        if (*p == '/') {
            last_slash = p;
        }
    }
    
    vfs_node_t *parent;
    if (last_slash) {
        strncpy(name, last_slash + 1, VFS_MAX_NAME - 1);
        name[VFS_MAX_NAME - 1] = '\0';
        
        if (last_slash == parent_path) {
            parent = vfs_root;
        } else {
            *last_slash = '\0';
            parent = vfs_lookup(parent_path);
        }
    } else {
        strncpy(name, path, VFS_MAX_NAME - 1);
        name[VFS_MAX_NAME - 1] = '\0';
        parent = vfs_root;
    }
    
    if (!parent || !(parent->flags & VFS_DIRECTORY)) {
        return -1;
    }
    
    return vfs_create(parent, name, VFS_DIRECTORY);
}

int vfs_mount(const char *path, vfs_node_t *fs_root)
{
    if (!path || !fs_root || mount_count >= MAX_MOUNTS) {
        return -1;
    }
    
    vfs_node_t *mount_point = vfs_lookup(path);
    if (!mount_point) {
        return -2;
    }
    
    if (!(mount_point->flags & VFS_DIRECTORY)) {
        return -3;
    }
    
    for (int i = 0; i < mount_count; i++) {
        if (strcmp(mount_table[i].path, path) == 0) {
            return -4;  
        }
    }
    
    strncpy(mount_table[mount_count].path, path, VFS_MAX_PATH - 1);
    mount_table[mount_count].mount_point = mount_point;
    mount_table[mount_count].fs_root = fs_root;
    
    mount_point->flags |= VFS_MOUNTPOINT;
    mount_point->ptr = fs_root;
    
    fs_root->parent = mount_point->parent;
    
    strncpy(fs_root->name, mount_point->name, VFS_MAX_NAME - 1);
    
    mount_count++;
    return 0;
}

int vfs_unmount(const char *path)
{
    for (int i = 0; i < mount_count; i++) {
        if (strcmp(mount_table[i].path, path) == 0) {
            mount_table[i].mount_point->flags &= ~VFS_MOUNTPOINT;
            mount_table[i].mount_point->ptr = NULL;
            
            for (int j = i; j < mount_count - 1; j++) {
                mount_table[j] = mount_table[j + 1];
            }
            mount_count--;
            return 0;
        }
    }
    return -1;
}

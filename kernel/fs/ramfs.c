/* RAM Filesystem
 * Simple in-memory filesystem
 */

#include <fs/ramfs.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <drivers/pit.h>
#include <string.h>

typedef struct ramfs_node {
    vfs_node_t vfs;             
    uint8_t *data;              
    uint32_t capacity;          
    struct ramfs_node **children; 
    uint32_t child_count;       
    uint32_t child_capacity;    
} ramfs_node_t;

static dirent_t ramfs_dirent;
static uint32_t next_inode = 1;

static int ramfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static int ramfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static dirent_t *ramfs_readdir(vfs_node_t *node, uint32_t index);
static vfs_node_t *ramfs_finddir(vfs_node_t *node, const char *name);
static int ramfs_create(vfs_node_t *node, const char *name, uint32_t type);
static int ramfs_unlink(vfs_node_t *node, const char *name);

static ramfs_node_t *ramfs_alloc_node(const char *name, uint32_t type)
{
    ramfs_node_t *node = kmalloc(sizeof(ramfs_node_t));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(ramfs_node_t));
    
    strncpy(node->vfs.name, name, VFS_MAX_NAME - 1);
    node->vfs.name[VFS_MAX_NAME - 1] = '\0';
    node->vfs.flags = type;
    node->vfs.inode = next_inode++;
    node->vfs.length = 0;
    
    uint32_t now = pit_get_ticks() / 100;
    node->vfs.ctime = now;
    node->vfs.mtime = now;
    node->vfs.atime = now;
    
    node->vfs.read = ramfs_read;
    node->vfs.write = ramfs_write;
    node->vfs.readdir = ramfs_readdir;
    node->vfs.finddir = ramfs_finddir;
    node->vfs.create = ramfs_create;
    node->vfs.unlink = ramfs_unlink;
    node->vfs.impl = node;
    
    if (type == VFS_DIRECTORY) {
        node->child_capacity = 8;
        node->children = kmalloc(sizeof(ramfs_node_t *) * node->child_capacity);
        if (node->children) {
            memset(node->children, 0, sizeof(ramfs_node_t *) * node->child_capacity);
        }
        node->child_count = 0;
    }
    
    return node;
}

static int ramfs_add_child(ramfs_node_t *parent, ramfs_node_t *child)
{
    if (!parent || !child || !(parent->vfs.flags & VFS_DIRECTORY)) {
        return -1;
    }
    
    if (parent->child_count >= parent->child_capacity) {
        uint32_t new_capacity = parent->child_capacity * 2;
        ramfs_node_t **new_children = kmalloc(sizeof(ramfs_node_t *) * new_capacity);
        if (!new_children) return -1;
        
        memcpy(new_children, parent->children, sizeof(ramfs_node_t *) * parent->child_count);
        kfree(parent->children);
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }
    
    parent->children[parent->child_count++] = child;
    child->vfs.parent = &parent->vfs;
    
    return 0;
}

static int ramfs_remove_child(ramfs_node_t *parent, ramfs_node_t *child)
{
    if (!parent || !child) return -1;
    
    for (uint32_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            for (uint32_t j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            return 0;
        }
    }
    
    return -1;
}

static int ramfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    ramfs_node_t *rnode = (ramfs_node_t *)node->impl;
    
    if (!rnode || !buffer || (node->flags & VFS_DIRECTORY)) {
        return -1;
    }
    
    if (offset >= node->length) {
        return 0;
    }
    
    uint32_t bytes_to_read = size;
    if (offset + size > node->length) {
        bytes_to_read = node->length - offset;
    }
    
    if (rnode->data) {
        memcpy(buffer, rnode->data + offset, bytes_to_read);
    }
    
    node->atime = pit_get_ticks() / 100;
    
    return bytes_to_read;
}

static int ramfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    ramfs_node_t *rnode = (ramfs_node_t *)node->impl;
    
    if (!rnode || !buffer || (node->flags & VFS_DIRECTORY)) {
        return -1;
    }
    
    uint32_t end_pos = offset + size;
    
    if (end_pos > RAMFS_MAX_FILE_SIZE) {
        return -1;
    }
    
    if (end_pos > rnode->capacity) {
        uint32_t new_capacity = end_pos + 1024;
        if (new_capacity > RAMFS_MAX_FILE_SIZE) {
            new_capacity = RAMFS_MAX_FILE_SIZE;
        }
        
        uint8_t *new_data = kmalloc(new_capacity);
        if (!new_data) return -1;
        
        memset(new_data, 0, new_capacity);
        if (rnode->data) {
            memcpy(new_data, rnode->data, rnode->capacity);
            kfree(rnode->data);
        }
        rnode->data = new_data;
        rnode->capacity = new_capacity;
    }
    
    memcpy(rnode->data + offset, buffer, size);
    
    if (end_pos > node->length) {
        node->length = end_pos;
    }
    
    node->mtime = pit_get_ticks() / 100;
    
    return size;
}

static dirent_t *ramfs_readdir(vfs_node_t *node, uint32_t index)
{
    ramfs_node_t *rnode = (ramfs_node_t *)node->impl;
    
    if (!rnode || !(node->flags & VFS_DIRECTORY)) {
        return NULL;
    }
    
    if (index >= rnode->child_count) {
        return NULL;
    }
    
    ramfs_node_t *child = rnode->children[index];
    strncpy(ramfs_dirent.name, child->vfs.name, VFS_MAX_NAME - 1);
    ramfs_dirent.name[VFS_MAX_NAME - 1] = '\0';
    ramfs_dirent.inode = child->vfs.inode;
    
    return &ramfs_dirent;
}

static vfs_node_t *ramfs_finddir(vfs_node_t *node, const char *name)
{
    ramfs_node_t *rnode = (ramfs_node_t *)node->impl;
    
    if (!rnode || !name || !(node->flags & VFS_DIRECTORY)) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < rnode->child_count; i++) {
        if (strcmp(rnode->children[i]->vfs.name, name) == 0) {
            return &rnode->children[i]->vfs;
        }
    }
    
    return NULL;
}

static int ramfs_create(vfs_node_t *node, const char *name, uint32_t type)
{
    ramfs_node_t *parent = (ramfs_node_t *)node->impl;
    
    if (!parent || !name || !(node->flags & VFS_DIRECTORY)) {
        return -1;
    }
    
    if (ramfs_finddir(node, name)) {
        return -1;
    }
    
    ramfs_node_t *child = ramfs_alloc_node(name, type);
    if (!child) return -1;
    
    return ramfs_add_child(parent, child);
}

static int ramfs_unlink(vfs_node_t *node, const char *name)
{
    ramfs_node_t *parent = (ramfs_node_t *)node->impl;
    
    if (!parent || !name || !(node->flags & VFS_DIRECTORY)) {
        return -1;
    }
    
    for (uint32_t i = 0; i < parent->child_count; i++) {
        if (strcmp(parent->children[i]->vfs.name, name) == 0) {
            ramfs_node_t *child = parent->children[i];
            
            if ((child->vfs.flags & VFS_DIRECTORY) && child->child_count > 0) {
                return -1;
            }
            
            ramfs_remove_child(parent, child);
            
            if (child->data) {
                kfree(child->data);
            }
            if (child->children) {
                kfree(child->children);
            }
            kfree(child);
            
            return 0;
        }
    }
    
    return -1;
}

vfs_node_t *ramfs_init(void)
{
    ramfs_node_t *root = ramfs_alloc_node("/", VFS_DIRECTORY);
    if (!root) return NULL;
    
    return &root->vfs;
}

vfs_node_t *ramfs_create_file(vfs_node_t *parent, const char *name)
{
    if (ramfs_create(parent, name, VFS_FILE) != 0) {
        return NULL;
    }
    return vfs_finddir(parent, name);
}

vfs_node_t *ramfs_create_dir(vfs_node_t *parent, const char *name)
{
    if (ramfs_create(parent, name, VFS_DIRECTORY) != 0) {
        return NULL;
    }
    return vfs_finddir(parent, name);
}

int ramfs_write_file(vfs_node_t *node, const void *data, uint32_t size)
{
    return ramfs_write(node, 0, size, (uint8_t *)data);
}

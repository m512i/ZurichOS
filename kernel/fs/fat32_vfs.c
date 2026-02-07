/* FAT32 VFS Wrapper
 * Provides VFS interface for FAT32 filesystem
 */

#include <fs/fat32.h>
#include <fs/vfs.h>
#include <drivers/ata.h>
#include <drivers/serial.h>
#include <mm/heap.h>
#include <string.h>

extern fat32_fs_t *fat32_mounted_drives[4];
void fat32_name_to_83(const char *name, char *name83);

typedef struct {
    fat32_fs_t *fs;
    uint32_t cluster;       
    uint32_t file_size;     
    uint8_t attr;           
} fat32_vfs_data_t;

static int fat32_vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static int fat32_vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static dirent_t *fat32_vfs_readdir(vfs_node_t *node, uint32_t index);
static vfs_node_t *fat32_vfs_finddir(vfs_node_t *node, const char *name);
static int fat32_vfs_create(vfs_node_t *node, const char *name, uint32_t type);
static int fat32_vfs_unlink(vfs_node_t *node, const char *name);
static dirent_t fat32_vfs_dirent;

typedef struct {
    char name[13];
    uint32_t size;
    uint8_t attr;
    uint32_t cluster;
} fat32_dir_cache_entry_t;

#define FAT32_DIR_CACHE_SIZE 64
static fat32_dir_cache_entry_t dir_cache[FAT32_DIR_CACHE_SIZE];
static int dir_cache_count = 0;
static uint32_t dir_cache_cluster = 0;
static fat32_fs_t *dir_cache_fs = NULL;

static void dir_cache_callback(const char *name, uint32_t size, uint8_t attr, void *ctx)
{
    (void)ctx;
    
    if (dir_cache_count < FAT32_DIR_CACHE_SIZE) {
        strncpy(dir_cache[dir_cache_count].name, name, 12);
        dir_cache[dir_cache_count].name[12] = '\0';
        dir_cache[dir_cache_count].size = size;
        dir_cache[dir_cache_count].attr = attr;
        dir_cache_count++;
    }
}

static vfs_node_t *fat32_create_vfs_node(fat32_fs_t *fs, const char *name, 
                                          uint32_t cluster, uint32_t size, uint8_t attr,
                                          vfs_node_t *parent)
{
    vfs_node_t *node = kmalloc(sizeof(vfs_node_t));
    if (!node) return NULL;
    
    fat32_vfs_data_t *data = kmalloc(sizeof(fat32_vfs_data_t));
    if (!data) {
        kfree(node);
        return NULL;
    }
    
    memset(node, 0, sizeof(vfs_node_t));
    
    strncpy(node->name, name, VFS_MAX_NAME - 1);
    node->name[VFS_MAX_NAME - 1] = '\0';
    
    if (attr & FAT32_ATTR_DIRECTORY) {
        node->flags = VFS_DIRECTORY;
    } else {
        node->flags = VFS_FILE;
    }
    
    node->length = size;
    node->inode = cluster;
    node->parent = parent;
    node->read = fat32_vfs_read;
    node->write = fat32_vfs_write;
    node->readdir = fat32_vfs_readdir;
    node->finddir = fat32_vfs_finddir;
    node->create = fat32_vfs_create;
    node->unlink = fat32_vfs_unlink;
    
    data->fs = fs;
    data->cluster = cluster;
    data->file_size = size;
    data->attr = attr;
    node->impl = data;
    
    return node;
}

static int fat32_vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    if (!node || !node->impl) return -1;
    
    fat32_vfs_data_t *data = (fat32_vfs_data_t *)node->impl;
    
    if (data->attr & FAT32_ATTR_DIRECTORY) {
        return -1;  
    }
    
    return fat32_read_file(data->fs, data->cluster, data->file_size, offset, size, buffer);
}

static int fat32_vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    if (!node || !node->impl) return -1;
    
    fat32_vfs_data_t *data = (fat32_vfs_data_t *)node->impl;
    
    if (data->attr & FAT32_ATTR_DIRECTORY) {
        return -1;  
    }
    
    uint32_t cluster = data->cluster;
    uint32_t file_size = data->file_size;
    
    int result = fat32_write_file(data->fs, &cluster, &file_size, offset, size, buffer);
    
    if (result > 0) {
        data->cluster = cluster;
        data->file_size = file_size;
        node->length = file_size;
        node->inode = cluster;
        
        if (node->parent && node->parent->impl) {
            fat32_vfs_data_t *parent_data = (fat32_vfs_data_t *)node->parent->impl;
            fat32_update_entry_size(data->fs, parent_data->cluster, node->name, file_size, cluster);
        }
        
        dir_cache_fs = NULL;
    }
    
    return result;
}

static dirent_t *fat32_vfs_readdir(vfs_node_t *node, uint32_t index)
{
    if (!node || !node->impl) return NULL;
    
    fat32_vfs_data_t *data = (fat32_vfs_data_t *)node->impl;
    
    if (!(data->attr & FAT32_ATTR_DIRECTORY)) {
        return NULL;
    }
    
    if (dir_cache_fs != data->fs || dir_cache_cluster != data->cluster) {
        dir_cache_count = 0;
        dir_cache_fs = data->fs;
        dir_cache_cluster = data->cluster;
        
        uint32_t cluster = data->cluster;
        fat32_list_dir(data->fs, cluster, dir_cache_callback, &cluster);
    }
    
    if (index >= (uint32_t)dir_cache_count) {
        return NULL;
    }
    
    strncpy(fat32_vfs_dirent.name, dir_cache[index].name, VFS_MAX_NAME - 1);
    fat32_vfs_dirent.inode = index;
    
    return &fat32_vfs_dirent;
}

static vfs_node_t *fat32_vfs_finddir(vfs_node_t *node, const char *name)
{
    if (!node || !node->impl || !name) return NULL;
    
    fat32_vfs_data_t *data = (fat32_vfs_data_t *)node->impl;
    
    if (!(data->attr & FAT32_ATTR_DIRECTORY)) {
        return NULL;
    }
    
    fat32_dirent_t entry;
    if (fat32_find_entry(data->fs, data->cluster, name, &entry) < 0) {
        return NULL;
    }
    
    uint32_t cluster = ((uint32_t)entry.cluster_hi << 16) | entry.cluster_lo;
    
    char formatted_name[13];
    int i, j = 0;
    for (i = 0; i < 8 && entry.name[i] != ' '; i++) {
        formatted_name[j++] = entry.name[i];
    }
    if (entry.ext[0] != ' ') {
        formatted_name[j++] = '.';
        for (i = 0; i < 3 && entry.ext[i] != ' '; i++) {
            formatted_name[j++] = entry.ext[i];
        }
    }
    formatted_name[j] = '\0';
    
    return fat32_create_vfs_node(data->fs, formatted_name, cluster, 
                                  entry.file_size, entry.attr, node);
}

static int fat32_vfs_create(vfs_node_t *node, const char *name, uint32_t type)
{
    if (!node || !node->impl || !name) return -1;
    
    fat32_vfs_data_t *data = (fat32_vfs_data_t *)node->impl;
    
    if (!(data->attr & FAT32_ATTR_DIRECTORY)) {
        return -1;  
    }
    
    fat32_dirent_t existing;
    if (fat32_find_entry(data->fs, data->cluster, name, &existing) == 0) {
        return -2;
    }
    
    uint8_t attr = (type == VFS_DIRECTORY) ? FAT32_ATTR_DIRECTORY : FAT32_ATTR_ARCHIVE;
    uint32_t new_cluster = 0;
    
    int result = fat32_create_entry(data->fs, data->cluster, name, attr, &new_cluster);
    
    dir_cache_fs = NULL;
    
    return result;
}

static int fat32_vfs_unlink(vfs_node_t *node, const char *name)
{
    if (!node || !node->impl || !name) return -1;
    
    fat32_vfs_data_t *data = (fat32_vfs_data_t *)node->impl;
    
    if (!(data->attr & FAT32_ATTR_DIRECTORY)) {
        return -1;
    }
    
    fat32_dirent_t entry;
    if (fat32_find_entry(data->fs, data->cluster, name, &entry) < 0) {
        return -1;
    }
    
    if (entry.attr & FAT32_ATTR_DIRECTORY) {
        return -2;
    }
    
    uint32_t cluster = ((uint32_t)entry.cluster_hi << 16) | entry.cluster_lo;
    if (cluster >= 2) {
        fat32_free_cluster_chain(data->fs, cluster);
    }
    
    uint8_t *cluster_buf = kmalloc(data->fs->sectors_per_cluster * 512);
    if (!cluster_buf) return -1;
    
    char name83[11];
    fat32_name_to_83(name, name83);
    
    uint32_t current_cluster = data->cluster;
    
    while (current_cluster != 0 && current_cluster < FAT32_CLUSTER_END) {
        if (fat32_read_cluster(data->fs, current_cluster, cluster_buf) < 0) {
            kfree(cluster_buf);
            return -1;
        }
        
        int entries_per_cluster = (data->fs->sectors_per_cluster * 512) / sizeof(fat32_dirent_t);
        fat32_dirent_t *entries = (fat32_dirent_t *)cluster_buf;
        
        for (int i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00) break;
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            
            if (memcmp(entries[i].name, name83, 8) == 0 &&
                memcmp(entries[i].ext, name83 + 8, 3) == 0) {
                /* Mark as deleted */
                entries[i].name[0] = 0xE5;
                fat32_write_cluster(data->fs, current_cluster, cluster_buf);
                kfree(cluster_buf);
                
                /* Invalidate cache */
                dir_cache_fs = NULL;
                return 0;
            }
        }
        
        current_cluster = fat32_next_cluster(data->fs, current_cluster);
    }
    
    kfree(cluster_buf);
    return -1;
}

vfs_node_t *fat32_get_vfs_root(fat32_fs_t *fs)
{
    if (!fs) return NULL;
    
    return fat32_create_vfs_node(fs, "fat32", fs->root_cluster, 0, 
                                  FAT32_ATTR_DIRECTORY, NULL);
}

fat32_fs_t *fat32_get_mounted(int drive)
{
    if (drive < 0 || drive >= 4) return NULL;
    return fat32_mounted_drives[drive];
}

void fat32_automount_all(void)
{
    serial_puts("[FAT32] Auto-mounting disks...\n");
    
    for (int i = 0; i < 4; i++) {
        ata_drive_t *drive = ata_get_drive(i);
        if (!drive) continue;
        
        if (drive->type != ATA_TYPE_ATA) continue;
        
        fat32_fs_t *fs = fat32_mount(i, 0);
        if (!fs) {
            serial_puts("[FAT32] hd");
            serial_putc('a' + i);
            serial_puts(": not FAT32\n");
            continue;
        }
        
        fat32_mounted_drives[i] = fs;
        
        char mount_path[16] = "/disks/hd";
        mount_path[9] = 'a' + i;
        mount_path[10] = '\0';
        
        vfs_node_t *fs_root = fat32_get_vfs_root(fs);
        if (!fs_root) {
            serial_puts("[FAT32] Failed to create VFS root for hd");
            serial_putc('a' + i);
            serial_puts("\n");
            continue;
        }
        
        if (vfs_mount(mount_path, fs_root) == 0) {
            serial_puts("[FAT32] Mounted hd");
            serial_putc('a' + i);
            serial_puts(" at ");
            serial_puts(mount_path);
            serial_puts(" (");
            serial_puts(fs->volume_label);
            serial_puts(")\n");
        } else {
            serial_puts("[FAT32] Failed to mount hd");
            serial_putc('a' + i);
            serial_puts("\n");
        }
    }
}

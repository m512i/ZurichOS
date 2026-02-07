/* FAT32 Filesystem Driver - Core Operations
 * Mount, read cluster, directory listing, file reading
 */

#include <fs/fat32.h>
#include <drivers/ata.h>
#include <mm/heap.h>
#include <string.h>
#include <drivers/serial.h>

uint8_t fat32_sector_buf[512];

fat32_fs_t *fat32_mounted_drives[4] = { NULL, NULL, NULL, NULL };

int fat32_read_sector_internal(fat32_fs_t *fs, uint32_t sector, void *buffer)
{
    return ata_read_sectors(fs->drive, fs->partition_lba + sector, 1, buffer);
}

int fat32_write_sector_internal(fat32_fs_t *fs, uint32_t sector, const void *buffer)
{
    return ata_write_sectors(fs->drive, fs->partition_lba + sector, 1, (void *)buffer);
}

static uint32_t cluster_to_lba(fat32_fs_t *fs, uint32_t cluster)
{
    return fs->data_start_lba + (cluster - 2) * fs->sectors_per_cluster;
}

fat32_fs_t *fat32_mount(int drive, uint32_t partition_lba)
{
    if (ata_read_sectors(drive, partition_lba, 1, fat32_sector_buf) < 0) {
        serial_puts("[FAT32] Failed to read boot sector\n");
        return NULL;
    }
    
    fat32_bpb_t *bpb = (fat32_bpb_t *)fat32_sector_buf;
    
    if (bpb->boot_signature != 0x29) {
        serial_puts("[FAT32] Invalid boot signature\n");
        return NULL;
    }
    
    if (bpb->fat_size_16 != 0) {
        serial_puts("[FAT32] Not FAT32 (FAT12/16?)\n");
        return NULL;
    }
    
    if (bpb->bytes_per_sector != 512) {
        serial_puts("[FAT32] Unsupported sector size\n");
        return NULL;
    }
    
    fat32_fs_t *fs = kmalloc(sizeof(fat32_fs_t));
    if (!fs) {
        return NULL;
    }
    
    memset(fs, 0, sizeof(fat32_fs_t));
    
    fs->drive = drive;
    fs->partition_lba = partition_lba;
    fs->bytes_per_sector = bpb->bytes_per_sector;
    fs->sectors_per_cluster = bpb->sectors_per_cluster;
    fs->fat_start_lba = bpb->reserved_sectors;
    fs->fat_size = bpb->fat_size_32;
    fs->root_cluster = bpb->root_cluster;
    fs->data_start_lba = bpb->reserved_sectors + (bpb->num_fats * bpb->fat_size_32);
    
    uint32_t data_sectors = bpb->total_sectors_32 - fs->data_start_lba;
    fs->total_clusters = data_sectors / bpb->sectors_per_cluster;
    
    memcpy(fs->volume_label, bpb->volume_label, 11);
    fs->volume_label[11] = '\0';
    
    for (int i = 10; i >= 0 && fs->volume_label[i] == ' '; i--) {
        fs->volume_label[i] = '\0';
    }
    
    serial_puts("[FAT32] Mounted: ");
    serial_puts(fs->volume_label);
    serial_puts("\n");
    
    return fs;
}

void fat32_unmount(fat32_fs_t *fs)
{
    if (fs) {
        kfree(fs);
    }
}

int fat32_read_cluster(fat32_fs_t *fs, uint32_t cluster, void *buffer)
{
    if (cluster < 2) {
        return -1;
    }
    
    uint32_t lba = cluster_to_lba(fs, cluster);
    uint8_t *buf = (uint8_t *)buffer;
    
    for (int i = 0; i < fs->sectors_per_cluster; i++) {
        if (ata_read_sectors(fs->drive, fs->partition_lba + lba + i, 1, buf) < 0) {
            return -1;
        }
        buf += 512;
    }
    
    return fs->sectors_per_cluster * 512;
}

uint32_t fat32_next_cluster(fat32_fs_t *fs, uint32_t cluster)
{
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs->fat_start_lba + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    if (fat32_read_sector_internal(fs, fat_sector, fat32_sector_buf) < 0) {
        return 0;
    }
    
    uint32_t next = *(uint32_t *)(fat32_sector_buf + entry_offset);
    next &= 0x0FFFFFFF;
    
    if (next >= FAT32_CLUSTER_END) {
        return 0;
    }
    
    return next;
}

static void format_83_name(const fat32_dirent_t *entry, char *out)
{
    int i, j = 0;
    
    for (i = 0; i < 8 && entry->name[i] != ' '; i++) {
        out[j++] = entry->name[i];
    }
    
    if (entry->ext[0] != ' ') {
        out[j++] = '.';
        for (i = 0; i < 3 && entry->ext[i] != ' '; i++) {
            out[j++] = entry->ext[i];
        }
    }
    
    out[j] = '\0';
}

int fat32_list_dir(fat32_fs_t *fs, uint32_t cluster,
                   void (*callback)(const char *name, uint32_t size, uint8_t attr, void *ctx),
                   void *ctx)
{
    uint8_t *cluster_buf = kmalloc(fs->sectors_per_cluster * 512);
    if (!cluster_buf) {
        return -1;
    }
    
    int count = 0;
    uint32_t current_cluster = cluster;
    
    while (current_cluster != 0 && current_cluster < FAT32_CLUSTER_END) {
        if (fat32_read_cluster(fs, current_cluster, cluster_buf) < 0) {
            kfree(cluster_buf);
            return -1;
        }
        
        int entries_per_cluster = (fs->sectors_per_cluster * 512) / sizeof(fat32_dirent_t);
        fat32_dirent_t *entries = (fat32_dirent_t *)cluster_buf;
        
        for (int i = 0; i < entries_per_cluster; i++) {
            fat32_dirent_t *entry = &entries[i];
            
            if (entry->name[0] == 0x00) {
                kfree(cluster_buf);
                return count;
            }
            
            if ((uint8_t)entry->name[0] == 0xE5) {
                continue;
            }
            
            if (entry->attr == FAT32_ATTR_LFN) {
                continue;
            }
            if (entry->attr & FAT32_ATTR_VOLUME_ID) {
                continue;
            }
            
            char name[13];
            format_83_name(entry, name);
            
            if (name[0] == '.') {
                continue;
            }
            
            if (callback) {
                callback(name, entry->file_size, entry->attr, ctx);
            }
            count++;
        }
        
        current_cluster = fat32_next_cluster(fs, current_cluster);
    }
    
    kfree(cluster_buf);
    return count;
}

static int name_match(const char *pattern, const fat32_dirent_t *entry)
{
    char entry_name[13];
    format_83_name(entry, entry_name);
    
    const char *p = pattern;
    const char *e = entry_name;
    
    while (*p && *e) {
        char pc = *p;
        char ec = *e;
        
        if (pc >= 'a' && pc <= 'z') pc -= 32;
        if (ec >= 'a' && ec <= 'z') ec -= 32;
        
        if (pc != ec) return 0;
        p++;
        e++;
    }
    
    return (*p == '\0' && *e == '\0');
}

int fat32_find_entry(fat32_fs_t *fs, uint32_t dir_cluster, const char *name,
                     fat32_dirent_t *out_entry)
{
    uint8_t *cluster_buf = kmalloc(fs->sectors_per_cluster * 512);
    if (!cluster_buf) {
        return -1;
    }
    
    uint32_t current_cluster = dir_cluster;
    
    while (current_cluster != 0 && current_cluster < FAT32_CLUSTER_END) {
        if (fat32_read_cluster(fs, current_cluster, cluster_buf) < 0) {
            kfree(cluster_buf);
            return -1;
        }
        
        int entries_per_cluster = (fs->sectors_per_cluster * 512) / sizeof(fat32_dirent_t);
        fat32_dirent_t *entries = (fat32_dirent_t *)cluster_buf;
        
        for (int i = 0; i < entries_per_cluster; i++) {
            fat32_dirent_t *entry = &entries[i];
            
            if (entry->name[0] == 0x00) {
                kfree(cluster_buf);
                return -1;
            }
            
            if ((uint8_t)entry->name[0] == 0xE5) continue;
            if (entry->attr == FAT32_ATTR_LFN) continue;
            if (entry->attr & FAT32_ATTR_VOLUME_ID) continue;
            
            if (name_match(name, entry)) {
                if (out_entry) {
                    memcpy(out_entry, entry, sizeof(fat32_dirent_t));
                }
                kfree(cluster_buf);
                return 0;
            }
        }
        
        current_cluster = fat32_next_cluster(fs, current_cluster);
    }
    
    kfree(cluster_buf);
    return -1;
}

int fat32_read_file(fat32_fs_t *fs, uint32_t start_cluster, uint32_t file_size,
                    uint32_t offset, uint32_t size, void *buffer)
{
    if (offset >= file_size) {
        return 0;
    }
    
    if (offset + size > file_size) {
        size = file_size - offset;
    }
    
    uint32_t cluster_size = fs->sectors_per_cluster * 512;
    uint8_t *cluster_buf = kmalloc(cluster_size);
    if (!cluster_buf) {
        return -1;
    }
    
    uint8_t *out = (uint8_t *)buffer;
    uint32_t bytes_read = 0;
    uint32_t current_cluster = start_cluster;
    uint32_t cluster_index = 0;
    
    uint32_t start_cluster_idx = offset / cluster_size;
    for (uint32_t i = 0; i < start_cluster_idx && current_cluster != 0; i++) {
        current_cluster = fat32_next_cluster(fs, current_cluster);
        cluster_index++;
    }
    
    uint32_t offset_in_cluster = offset % cluster_size;
    
    while (bytes_read < size && current_cluster != 0 && current_cluster < FAT32_CLUSTER_END) {
        if (fat32_read_cluster(fs, current_cluster, cluster_buf) < 0) {
            kfree(cluster_buf);
            return -1;
        }
        
        uint32_t copy_start = (cluster_index == start_cluster_idx) ? offset_in_cluster : 0;
        uint32_t copy_size = cluster_size - copy_start;
        
        if (copy_size > size - bytes_read) {
            copy_size = size - bytes_read;
        }
        
        memcpy(out + bytes_read, cluster_buf + copy_start, copy_size);
        bytes_read += copy_size;
        
        current_cluster = fat32_next_cluster(fs, current_cluster);
        cluster_index++;
    }
    
    kfree(cluster_buf);
    return bytes_read;
}

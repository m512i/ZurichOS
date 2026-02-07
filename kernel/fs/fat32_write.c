/* FAT32 Write Operations
 * Cluster allocation, file writing, directory entry creation
 */

#include <fs/fat32.h>
#include <drivers/ata.h>
#include <mm/heap.h>
#include <string.h>

extern uint8_t fat32_sector_buf[512];
int fat32_read_sector_internal(fat32_fs_t *fs, uint32_t sector, void *buffer);
int fat32_write_sector_internal(fat32_fs_t *fs, uint32_t sector, const void *buffer);

void fat32_name_to_83(const char *name, char *name83)
{
    memset(name83, ' ', 11);
    
    int i = 0, j = 0;
    
    while (name[i] && name[i] != '.' && j < 8) {
        char c = name[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        name83[j++] = c;
    }
    
    while (name[i] && name[i] != '.') i++;
    
    if (name[i] == '.') {
        i++;
        j = 8;
        while (name[i] && j < 11) {
            char c = name[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            name83[j++] = c;
        }
    }
}

uint32_t fat32_alloc_cluster(fat32_fs_t *fs)
{
    uint8_t fat_buf[512];
    
    for (uint32_t cluster = 2; cluster < fs->total_clusters + 2; cluster++) {
        uint32_t fat_offset = cluster * 4;
        uint32_t fat_sector = fs->fat_start_lba + (fat_offset / 512);
        uint32_t entry_offset = fat_offset % 512;
        
        if (fat32_read_sector_internal(fs, fat_sector, fat_buf) < 0) {
            continue;
        }
        
        uint32_t value = *(uint32_t *)(fat_buf + entry_offset) & 0x0FFFFFFF;
        
        if (value == FAT32_CLUSTER_FREE) {
            *(uint32_t *)(fat_buf + entry_offset) = 0x0FFFFFFF;
            fat32_write_sector_internal(fs, fat_sector, fat_buf);
            return cluster;
        }
    }
    
    return 0;
}

int fat32_set_cluster(fat32_fs_t *fs, uint32_t cluster, uint32_t value)
{
    uint8_t fat_buf[512];
    
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs->fat_start_lba + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    if (fat32_read_sector_internal(fs, fat_sector, fat_buf) < 0) {
        return -1;
    }
    
    uint32_t old_value = *(uint32_t *)(fat_buf + entry_offset);
    *(uint32_t *)(fat_buf + entry_offset) = (old_value & 0xF0000000) | (value & 0x0FFFFFFF);
    
    return fat32_write_sector_internal(fs, fat_sector, fat_buf);
}

int fat32_free_cluster_chain(fat32_fs_t *fs, uint32_t start_cluster)
{
    uint32_t cluster = start_cluster;
    
    while (cluster >= 2 && cluster < FAT32_CLUSTER_END) {
        uint32_t next = fat32_next_cluster(fs, cluster);
        fat32_set_cluster(fs, cluster, FAT32_CLUSTER_FREE);
        cluster = next;
    }
    
    return 0;
}

int fat32_write_cluster(fat32_fs_t *fs, uint32_t cluster, const void *buffer)
{
    if (cluster < 2) {
        return -1;
    }
    
    uint32_t lba = fs->data_start_lba + (cluster - 2) * fs->sectors_per_cluster;
    const uint8_t *buf = (const uint8_t *)buffer;
    
    for (int i = 0; i < fs->sectors_per_cluster; i++) {
        if (ata_write_sectors(fs->drive, fs->partition_lba + lba + i, 1, (void *)(buf + i * 512)) < 0) {
            return -1;
        }
    }
    
    return fs->sectors_per_cluster * 512;
}

int fat32_create_entry(fat32_fs_t *fs, uint32_t dir_cluster, const char *name, 
                       uint8_t attr, uint32_t *out_cluster)
{
    uint8_t *cluster_buf = kmalloc(fs->sectors_per_cluster * 512);
    if (!cluster_buf) return -1;
    
    uint32_t current_cluster = dir_cluster;
    
    char name83[11];
    fat32_name_to_83(name, name83);
    
    while (current_cluster != 0 && current_cluster < FAT32_CLUSTER_END) {
        if (fat32_read_cluster(fs, current_cluster, cluster_buf) < 0) {
            kfree(cluster_buf);
            return -1;
        }
        
        int entries_per_cluster = (fs->sectors_per_cluster * 512) / sizeof(fat32_dirent_t);
        fat32_dirent_t *entries = (fat32_dirent_t *)cluster_buf;
        
        for (int i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
                memset(&entries[i], 0, sizeof(fat32_dirent_t));
                memcpy(entries[i].name, name83, 8);
                memcpy(entries[i].ext, name83 + 8, 3);
                entries[i].attr = attr;
                
                uint32_t new_cluster = 0;
                if (attr & FAT32_ATTR_DIRECTORY) {
                    new_cluster = fat32_alloc_cluster(fs);
                    if (new_cluster == 0) {
                        kfree(cluster_buf);
                        return -2;
                    }

                    uint8_t *zero_buf = kmalloc(fs->sectors_per_cluster * 512);
                    if (zero_buf) {
                        memset(zero_buf, 0, fs->sectors_per_cluster * 512);
                        fat32_write_cluster(fs, new_cluster, zero_buf);
                        kfree(zero_buf);
                    }
                }
                
                entries[i].cluster_hi = (new_cluster >> 16) & 0xFFFF;
                entries[i].cluster_lo = new_cluster & 0xFFFF;
                entries[i].file_size = 0;
                
                fat32_write_cluster(fs, current_cluster, cluster_buf);
                
                if (out_cluster) *out_cluster = new_cluster;
                
                kfree(cluster_buf);
                return 0;
            }
        }
        
        uint32_t next = fat32_next_cluster(fs, current_cluster);
        if (next == 0) {
            next = fat32_alloc_cluster(fs);
            if (next == 0) {
                kfree(cluster_buf);
                return -2;
            }
            fat32_set_cluster(fs, current_cluster, next);
            
            memset(cluster_buf, 0, fs->sectors_per_cluster * 512);
            fat32_write_cluster(fs, next, cluster_buf);
        }
        current_cluster = next;
    }
    
    kfree(cluster_buf);
    return -1;
}

int fat32_update_entry_size(fat32_fs_t *fs, uint32_t dir_cluster, 
                            const char *name, uint32_t new_size, uint32_t new_cluster)
{
    uint8_t *cluster_buf = kmalloc(fs->sectors_per_cluster * 512);
    if (!cluster_buf) return -1;
    
    char name83[11];
    fat32_name_to_83(name, name83);
    
    uint32_t current_cluster = dir_cluster;
    
    while (current_cluster != 0 && current_cluster < FAT32_CLUSTER_END) {
        if (fat32_read_cluster(fs, current_cluster, cluster_buf) < 0) {
            kfree(cluster_buf);
            return -1;
        }
        
        int entries_per_cluster = (fs->sectors_per_cluster * 512) / sizeof(fat32_dirent_t);
        fat32_dirent_t *entries = (fat32_dirent_t *)cluster_buf;
        
        for (int i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00) {
                kfree(cluster_buf);
                return -1;
            }
            
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            if (entries[i].attr == FAT32_ATTR_LFN) continue;
            
            if (memcmp(entries[i].name, name83, 8) == 0 &&
                memcmp(entries[i].ext, name83 + 8, 3) == 0) {
                entries[i].file_size = new_size;
                if (new_cluster != 0) {
                    entries[i].cluster_hi = (new_cluster >> 16) & 0xFFFF;
                    entries[i].cluster_lo = new_cluster & 0xFFFF;
                }
                
                fat32_write_cluster(fs, current_cluster, cluster_buf);
                kfree(cluster_buf);
                return 0;
            }
        }
        
        current_cluster = fat32_next_cluster(fs, current_cluster);
    }
    
    kfree(cluster_buf);
    return -1;
}

int fat32_write_file(fat32_fs_t *fs, uint32_t *start_cluster, uint32_t *file_size,
                     uint32_t offset, uint32_t size, const void *buffer)
{
    if (size == 0) return 0;
    
    uint32_t cluster_size = fs->sectors_per_cluster * 512;
    uint8_t *cluster_buf = kmalloc(cluster_size);
    if (!cluster_buf) return -1;
    
    const uint8_t *src = (const uint8_t *)buffer;
    uint32_t bytes_written = 0;
    
    if (*start_cluster == 0) {
        *start_cluster = fat32_alloc_cluster(fs);
        if (*start_cluster == 0) {
            kfree(cluster_buf);
            return -1;
        }
    }
    
    uint32_t current_cluster = *start_cluster;
    uint32_t cluster_index = 0;
    uint32_t target_cluster_idx = offset / cluster_size;
    
    while (cluster_index < target_cluster_idx) {
        uint32_t next = fat32_next_cluster(fs, current_cluster);
        if (next == 0 || next >= FAT32_CLUSTER_END) {
            next = fat32_alloc_cluster(fs);
            if (next == 0) {
                kfree(cluster_buf);
                return bytes_written;
            }
            fat32_set_cluster(fs, current_cluster, next);
        }
        current_cluster = next;
        cluster_index++;
    }
    
    uint32_t offset_in_cluster = offset % cluster_size;
    
    while (bytes_written < size) {
        if (fat32_read_cluster(fs, current_cluster, cluster_buf) < 0) {
            memset(cluster_buf, 0, cluster_size);
        }
        
        uint32_t write_start = (cluster_index == target_cluster_idx) ? offset_in_cluster : 0;
        uint32_t write_size = cluster_size - write_start;
        if (write_size > size - bytes_written) {
            write_size = size - bytes_written;
        }
        
        memcpy(cluster_buf + write_start, src + bytes_written, write_size);
        
        if (fat32_write_cluster(fs, current_cluster, cluster_buf) < 0) {
            kfree(cluster_buf);
            return bytes_written;
        }
        
        bytes_written += write_size;
        
        if (bytes_written < size) {
            uint32_t next = fat32_next_cluster(fs, current_cluster);
            if (next == 0 || next >= FAT32_CLUSTER_END) {
                next = fat32_alloc_cluster(fs);
                if (next == 0) {
                    kfree(cluster_buf);
                    return bytes_written;
                }
                fat32_set_cluster(fs, current_cluster, next);
            }
            current_cluster = next;
            cluster_index++;
        }
    }
    
    if (offset + bytes_written > *file_size) {
        *file_size = offset + bytes_written;
    }
    
    kfree(cluster_buf);
    return bytes_written;
}

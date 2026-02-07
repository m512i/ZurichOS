#ifndef _FS_FAT32_H
#define _FS_FAT32_H

#include <stdint.h>
#include <stdbool.h>
#include <fs/vfs.h>

typedef struct __attribute__((packed)) {
    uint8_t  jmp[3];            
    char     oem_name[8];       
    uint16_t bytes_per_sector;  
    uint8_t  sectors_per_cluster; 
    uint16_t reserved_sectors;  
    uint8_t  num_fats;          
    uint16_t root_entry_count;  
    uint16_t total_sectors_16;  
    uint8_t  media_type;        
    uint16_t fat_size_16;       
    uint16_t sectors_per_track; 
    uint16_t num_heads;         
    uint32_t hidden_sectors;    
    uint32_t total_sectors_32;  

    uint32_t fat_size_32;       
    uint16_t ext_flags;         
    uint16_t fs_version;        
    uint32_t root_cluster;      
    uint16_t fs_info_sector;    
    uint16_t backup_boot_sector; 
    uint8_t  reserved[12];     
    uint8_t  drive_number;      
    uint8_t  reserved1;         
    uint8_t  boot_signature;    
    uint32_t volume_id;         
    char     volume_label[11];  
    char     fs_type[8];        
} fat32_bpb_t;

typedef struct __attribute__((packed)) {
    char     name[8];           
    char     ext[3];            
    uint8_t  attr;              
    uint8_t  nt_reserved;       
    uint8_t  create_time_tenth; 
    uint16_t create_time;       
    uint16_t create_date;       
    uint16_t access_date;      
    uint16_t cluster_hi;        
    uint16_t modify_time;       
    uint16_t modify_date;       
    uint16_t cluster_lo;        
    uint32_t file_size;         
} fat32_dirent_t;

typedef struct __attribute__((packed)) {
    uint8_t  order;             
    uint16_t name1[5];          
    uint8_t  attr;              
    uint8_t  type;              
    uint8_t  checksum;          
    uint16_t name2[6];          
    uint16_t cluster;           
    uint16_t name3[2];          
} fat32_lfn_t;

#define FAT32_ATTR_READ_ONLY    0x01
#define FAT32_ATTR_HIDDEN       0x02
#define FAT32_ATTR_SYSTEM       0x04
#define FAT32_ATTR_VOLUME_ID    0x08
#define FAT32_ATTR_DIRECTORY    0x10
#define FAT32_ATTR_ARCHIVE      0x20
#define FAT32_ATTR_LFN          0x0F  

#define FAT32_CLUSTER_FREE      0x00000000
#define FAT32_CLUSTER_RESERVED  0x00000001
#define FAT32_CLUSTER_BAD       0x0FFFFFF7
#define FAT32_CLUSTER_END       0x0FFFFFF8  

typedef struct {
    int drive;                  
    uint32_t partition_lba;     
    
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint32_t fat_start_lba;     
    uint32_t fat_size;          
    uint32_t data_start_lba;    
    uint32_t root_cluster;      
    uint32_t total_clusters;    
    char volume_label[12];
} fat32_fs_t;

fat32_fs_t *fat32_mount(int drive, uint32_t partition_lba);
void fat32_unmount(fat32_fs_t *fs);
int fat32_read_cluster(fat32_fs_t *fs, uint32_t cluster, void *buffer);
uint32_t fat32_next_cluster(fat32_fs_t *fs, uint32_t cluster);

int fat32_list_dir(fat32_fs_t *fs, uint32_t cluster, 
                   void (*callback)(const char *name, uint32_t size, uint8_t attr, void *ctx),
                   void *ctx);

int fat32_find_entry(fat32_fs_t *fs, uint32_t dir_cluster, const char *name,
                     fat32_dirent_t *out_entry);

int fat32_read_file(fat32_fs_t *fs, uint32_t start_cluster, uint32_t file_size,
                    uint32_t offset, uint32_t size, void *buffer);

uint32_t fat32_alloc_cluster(fat32_fs_t *fs);
int fat32_free_cluster_chain(fat32_fs_t *fs, uint32_t start_cluster);
int fat32_set_cluster(fat32_fs_t *fs, uint32_t cluster, uint32_t value);
int fat32_write_cluster(fat32_fs_t *fs, uint32_t cluster, const void *buffer);
int fat32_create_entry(fat32_fs_t *fs, uint32_t dir_cluster, const char *name, 
                       uint8_t attr, uint32_t *out_cluster);
int fat32_write_file(fat32_fs_t *fs, uint32_t *start_cluster, uint32_t *file_size,
                     uint32_t offset, uint32_t size, const void *buffer);
int fat32_update_entry_size(fat32_fs_t *fs, uint32_t dir_cluster, 
                            const char *name, uint32_t new_size, uint32_t new_cluster);

vfs_node_t *fat32_get_vfs_root(fat32_fs_t *fs);
void fat32_automount_all(void);
fat32_fs_t *fat32_get_mounted(int drive);

#endif

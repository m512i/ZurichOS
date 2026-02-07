/* Shell Commands - Disk Category
 * lsblk, hdinfo, readsec, fatmount, fatls, fatcat, mounts
 */

#include <shell/builtins.h>
#include <kernel/shell.h>
#include <kernel/kernel.h>
#include <drivers/vga.h>
#include <drivers/ata.h>
#include <fs/vfs.h>
#include <fs/fat32.h>
#include <mm/heap.h>
#include <string.h>

static fat32_fs_t *mounted_fat32 = NULL;

void cmd_lsblk(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    int count = ata_get_drive_count();
    
    if (count == 0) {
        vga_puts("No block devices found\n");
        return;
    }
    
    vga_puts("NAME    TYPE   SIZE       MODEL\n");
    vga_puts("----    ----   ----       -----\n");
    
    for (int i = 0; i < 4; i++) {
        ata_drive_t *drive = ata_get_drive(i);
        if (!drive) continue;
        
        vga_puts("hd");
        vga_putchar('a' + i);
        vga_puts("     ");
        
        if (drive->type == ATA_TYPE_ATA) {
            vga_puts("ATA    ");
        } else {
            vga_puts("ATAPI  ");
        }
        
        if (drive->size_mb >= 1024) {
            vga_put_dec(drive->size_mb / 1024);
            vga_puts(" GB     ");
        } else {
            vga_put_dec(drive->size_mb);
            vga_puts(" MB     ");
        }
        
        vga_puts(drive->model);
        vga_puts("\n");
    }
    
    vga_puts("\nTotal: ");
    vga_put_dec(count);
    vga_puts(" device(s)\n");
}

void cmd_hdinfo(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: hdinfo <drive>\n");
        vga_puts("  drive: hda, hdb, hdc, hdd (or 0-3)\n");
        return;
    }
    
    int idx;
    if (argv[1][0] == 'h' && argv[1][1] == 'd') {
        idx = argv[1][2] - 'a';
    } else {
        idx = shell_parse_dec(argv[1]);
    }
    
    ata_drive_t *drive = ata_get_drive(idx);
    if (!drive) {
        vga_puts("hdinfo: drive not found\n");
        return;
    }
    
    vga_puts("Drive hd");
    vga_putchar('a' + idx);
    vga_puts(":\n");
    
    vga_puts("  Model:    ");
    vga_puts(drive->model);
    vga_puts("\n");
    
    vga_puts("  Serial:   ");
    vga_puts(drive->serial);
    vga_puts("\n");
    
    vga_puts("  Firmware: ");
    vga_puts(drive->firmware);
    vga_puts("\n");
    
    vga_puts("  Type:     ");
    vga_puts(drive->type == ATA_TYPE_ATA ? "ATA" : "ATAPI");
    vga_puts("\n");
    
    vga_puts("  Channel:  ");
    vga_puts(drive->channel == 0 ? "Primary" : "Secondary");
    vga_puts(drive->drive == 0 ? " Master" : " Slave");
    vga_puts("\n");
    
    vga_puts("  LBA48:    ");
    vga_puts(drive->lba48 ? "Yes" : "No");
    vga_puts("\n");
    
    vga_puts("  Sectors:  ");
    vga_put_dec(drive->sectors);
    vga_puts("\n");
    
    vga_puts("  Size:     ");
    if (drive->size_mb >= 1024) {
        vga_put_dec(drive->size_mb / 1024);
        vga_puts(" GB (");
        vga_put_dec(drive->size_mb);
        vga_puts(" MB)\n");
    } else {
        vga_put_dec(drive->size_mb);
        vga_puts(" MB\n");
    }
}

void cmd_readsec(int argc, char **argv)
{
    if (argc < 3) {
        vga_puts("Usage: readsec <drive> <lba>\n");
        vga_puts("  drive: hda, hdb, hdc, hdd (or 0-3)\n");
        vga_puts("  lba: sector number (0 = boot sector)\n");
        return;
    }
    
    int idx;
    if (argv[1][0] == 'h' && argv[1][1] == 'd') {
        idx = argv[1][2] - 'a';
    } else {
        idx = shell_parse_dec(argv[1]);
    }
    
    uint32_t lba = shell_parse_dec(argv[2]);
    
    ata_drive_t *drive = ata_get_drive(idx);
    if (!drive) {
        vga_puts("readsec: drive not found\n");
        return;
    }
    
    if (drive->type != ATA_TYPE_ATA) {
        vga_puts("readsec: only ATA drives supported\n");
        return;
    }
    
    uint8_t *buffer = kmalloc(512);
    if (!buffer) {
        vga_puts("readsec: out of memory\n");
        return;
    }
    
    vga_puts("Reading sector ");
    vga_put_dec(lba);
    vga_puts(" from hd");
    vga_putchar('a' + idx);
    vga_puts("...\n");
    
    int result = ata_read_sectors(idx, lba, 1, buffer);
    if (result < 0) {
        vga_puts("readsec: read failed (error ");
        vga_put_dec(-result);
        vga_puts(")\n");
        kfree(buffer);
        return;
    }
    
    /* Hexdump first 256 bytes */
    vga_puts("\nOffset    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F\n");
    vga_puts("--------  -----------------------------------------------\n");
    
    for (int row = 0; row < 16; row++) {
        vga_put_hex(row * 16);
        vga_puts("  ");
        
        for (int col = 0; col < 16; col++) {
            uint8_t b = buffer[row * 16 + col];
            if (b < 16) vga_putchar('0');
            vga_put_hex(b);
            vga_putchar(' ');
            if (col == 7) vga_putchar(' ');
        }
        
        vga_puts(" ");
        
        for (int col = 0; col < 16; col++) {
            uint8_t b = buffer[row * 16 + col];
            if (b >= 32 && b < 127) {
                vga_putchar(b);
            } else {
                vga_putchar('.');
            }
        }
        
        vga_puts("\n");
    }
    
    vga_puts("\n(showing first 256 of 512 bytes)\n");
    
    kfree(buffer);
}

void cmd_fatmount(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: fatmount <drive> [mountpoint]\n");
        vga_puts("  drive: hda, hdb, hdc, hdd (or 0-3)\n");
        vga_puts("  mountpoint: /mnt (default)\n");
        return;
    }
    
    int idx;
    if (argv[1][0] == 'h' && argv[1][1] == 'd') {
        idx = argv[1][2] - 'a';
    } else {
        idx = shell_parse_dec(argv[1]);
    }
    
    const char *mount_path = "/mnt";
    if (argc >= 3) {
        mount_path = argv[2];
    }
    
    ata_drive_t *drive = ata_get_drive(idx);
    if (!drive) {
        vga_puts("fatmount: drive not found\n");
        return;
    }
    
    if (drive->type != ATA_TYPE_ATA) {
        vga_puts("fatmount: only ATA drives supported\n");
        return;
    }
    
    if (mounted_fat32) {
        vfs_unmount(mount_path);
        fat32_unmount(mounted_fat32);
        mounted_fat32 = NULL;
    }
    
    mounted_fat32 = fat32_mount(idx, 0);
    
    if (!mounted_fat32) {
        vga_puts("fatmount: failed to mount FAT32\n");
        vga_puts("  (Is the disk formatted as FAT32?)\n");
        return;
    }
    
    vfs_node_t *fat32_root = fat32_get_vfs_root(mounted_fat32);
    if (!fat32_root) {
        vga_puts("fatmount: failed to create VFS root\n");
        fat32_unmount(mounted_fat32);
        mounted_fat32 = NULL;
        return;
    }
    
    int result = vfs_mount(mount_path, fat32_root);
    if (result < 0) {
        vga_puts("fatmount: failed to mount at ");
        vga_puts(mount_path);
        vga_puts(" (error ");
        vga_put_dec(-result);
        vga_puts(")\n");
        fat32_unmount(mounted_fat32);
        mounted_fat32 = NULL;
        return;
    }
    
    vga_puts("Mounted FAT32 from hd");
    vga_putchar('a' + idx);
    vga_puts(" at ");
    vga_puts(mount_path);
    vga_puts("\n  Volume: ");
    vga_puts(mounted_fat32->volume_label);
    vga_puts("\n  Use 'cd ");
    vga_puts(mount_path);
    vga_puts("' to access files\n");
}

static void fatls_callback(const char *name, uint32_t size, uint8_t attr, void *ctx)
{
    (void)ctx;
    
    if (attr & FAT32_ATTR_DIRECTORY) {
        vga_puts("[DIR]  ");
    } else {
        vga_puts("       ");
    }
    
    if (!(attr & FAT32_ATTR_DIRECTORY)) {
        char size_buf[12];
        int len = 0;
        uint32_t s = size;
        
        if (s == 0) {
            size_buf[len++] = '0';
        } else {
            while (s > 0) {
                size_buf[len++] = '0' + (s % 10);
                s /= 10;
            }
        }
        
        for (int i = len; i < 10; i++) {
            vga_putchar(' ');
        }
        
        for (int i = len - 1; i >= 0; i--) {
            vga_putchar(size_buf[i]);
        }
        vga_puts("  ");
    } else {
        vga_puts("           ");
    }
    
    vga_puts(name);
    vga_puts("\n");
}

void cmd_fatls(int argc, char **argv)
{
    if (!mounted_fat32) {
        vga_puts("fatls: no FAT32 mounted (use fatmount first)\n");
        return;
    }
    
    uint32_t dir_cluster = mounted_fat32->root_cluster;
    const char *path = "/";
    
    if (argc >= 2) {
        path = argv[1];
        
        char path_copy[256];
        strncpy(path_copy, argv[1], 255);
        
        char *token = path_copy;
        char *next;
        
        if (*token == '/') token++;
        
        while (*token) {
            next = token;
            while (*next && *next != '/') next++;
            
            if (*next == '/') {
                *next = '\0';
                next++;
            }
            
            if (*token == '\0') break;
            
            fat32_dirent_t entry;
            if (fat32_find_entry(mounted_fat32, dir_cluster, token, &entry) < 0) {
                vga_puts("fatls: not found: ");
                vga_puts(token);
                vga_puts("\n");
                return;
            }
            
            if (!(entry.attr & FAT32_ATTR_DIRECTORY)) {
                vga_puts("fatls: not a directory: ");
                vga_puts(token);
                vga_puts("\n");
                return;
            }
            
            dir_cluster = ((uint32_t)entry.cluster_hi << 16) | entry.cluster_lo;
            token = next;
        }
    }
    
    vga_puts("Directory: ");
    vga_puts(path);
    vga_puts("\n\n");
    
    int count = fat32_list_dir(mounted_fat32, dir_cluster, fatls_callback, NULL);
    
    if (count < 0) {
        vga_puts("fatls: error reading directory\n");
    } else {
        vga_puts("\n");
        vga_put_dec(count);
        vga_puts(" item(s)\n");
    }
}

void cmd_fatcat(int argc, char **argv)
{
    if (!mounted_fat32) {
        vga_puts("fatcat: no FAT32 mounted (use fatmount first)\n");
        return;
    }
    
    if (argc < 2) {
        vga_puts("Usage: fatcat <file>\n");
        return;
    }
    
    char path_copy[256];
    strncpy(path_copy, argv[1], 255);
    
    uint32_t dir_cluster = mounted_fat32->root_cluster;
    char *filename = path_copy;
    
    if (*filename == '/') filename++;
    
    char *last_slash = NULL;
    for (char *p = filename; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    
    if (last_slash) {
        *last_slash = '\0';
        char *dir_path = filename;
        filename = last_slash + 1;
        
        char *token = dir_path;
        char *next;
        
        while (*token) {
            next = token;
            while (*next && *next != '/') next++;
            
            if (*next == '/') {
                *next = '\0';
                next++;
            }
            
            if (*token == '\0') break;
            
            fat32_dirent_t entry;
            if (fat32_find_entry(mounted_fat32, dir_cluster, token, &entry) < 0) {
                vga_puts("fatcat: directory not found: ");
                vga_puts(token);
                vga_puts("\n");
                return;
            }
            
            if (!(entry.attr & FAT32_ATTR_DIRECTORY)) {
                vga_puts("fatcat: not a directory: ");
                vga_puts(token);
                vga_puts("\n");
                return;
            }
            
            dir_cluster = ((uint32_t)entry.cluster_hi << 16) | entry.cluster_lo;
            token = next;
        }
    }
    
    fat32_dirent_t entry;
    if (fat32_find_entry(mounted_fat32, dir_cluster, filename, &entry) < 0) {
        vga_puts("fatcat: file not found: ");
        vga_puts(filename);
        vga_puts("\n");
        return;
    }
    
    if (entry.attr & FAT32_ATTR_DIRECTORY) {
        vga_puts("fatcat: is a directory: ");
        vga_puts(filename);
        vga_puts("\n");
        return;
    }
    
    uint32_t file_cluster = ((uint32_t)entry.cluster_hi << 16) | entry.cluster_lo;
    uint32_t file_size = entry.file_size;
    
    if (file_size == 0) {
        vga_puts("(empty file)\n");
        return;
    }
    
    uint32_t display_size = file_size;
    if (display_size > 4096) {
        display_size = 4096;
    }
    
    uint8_t *buffer = kmalloc(display_size + 1);
    if (!buffer) {
        vga_puts("fatcat: out of memory\n");
        return;
    }
    
    int bytes_read = fat32_read_file(mounted_fat32, file_cluster, file_size, 0, display_size, buffer);
    
    if (bytes_read < 0) {
        vga_puts("fatcat: read error\n");
        kfree(buffer);
        return;
    }
    
    buffer[bytes_read] = '\0';
    
    for (int i = 0; i < bytes_read; i++) {
        if (buffer[i] == '\r') continue; 
        vga_putchar(buffer[i]);
    }
    
    if (file_size > display_size) {
        vga_puts("\n\n... (truncated, showing ");
        vga_put_dec(display_size);
        vga_puts(" of ");
        vga_put_dec(file_size);
        vga_puts(" bytes)\n");
    }
    
    kfree(buffer);
}

void cmd_mounts(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("Mounted filesystems:\n");
    vga_puts("DRIVE   MOUNT POINT      VOLUME\n");
    vga_puts("-----   -----------      ------\n");
    
    int count = 0;
    
    for (int i = 0; i < 4; i++) {
        fat32_fs_t *fs = fat32_get_mounted(i);
        if (!fs) continue;
        
        vga_puts("hd");
        vga_putchar('a' + i);
        vga_puts("     /disks/hd");
        vga_putchar('a' + i);
        vga_puts("       ");
        vga_puts(fs->volume_label[0] ? fs->volume_label : "(no label)");
        vga_puts("\n");
        count++;
    }
    
    if (count == 0) {
        vga_puts("(no FAT32 filesystems mounted)\n");
    } else {
        vga_puts("\nTotal: ");
        vga_put_dec(count);
        vga_puts(" filesystem(s)\n");
    }
}

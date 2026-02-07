/* Device Filesystem (devfs)
 * Virtual filesystem for device nodes
 */

#include <fs/vfs.h>
#include <fs/ramfs.h>
#include <mm/heap.h>
#include <string.h>
#include <drivers/serial.h>

#define DEV_TYPE_CHAR   1
#define DEV_TYPE_BLOCK  2

typedef struct {
    char name[32];
    int type;           
    int major;
    int minor;
    uint32_t (*read)(void *buf, uint32_t size);
    uint32_t (*write)(const void *buf, uint32_t size);
} device_node_t;

#define MAX_DEVICES 64
static device_node_t devices[MAX_DEVICES];
static int device_count = 0;

static vfs_node_t *devfs_root = NULL;

static uint32_t null_read(void *buf, uint32_t size)
{
    (void)buf;
    (void)size;
    return 0;  
}

static uint32_t null_write(const void *buf, uint32_t size)
{
    (void)buf;
    return size;  
}

static uint32_t zero_read(void *buf, uint32_t size)
{
    memset(buf, 0, size);
    return size;
}

static uint32_t zero_write(const void *buf, uint32_t size)
{
    (void)buf;
    return size;
}

static uint32_t random_seed = 0x12345678;
static uint32_t random_read(void *buf, uint32_t size)
{
    uint8_t *p = (uint8_t *)buf;
    for (uint32_t i = 0; i < size; i++) {
        random_seed = random_seed * 1103515245 + 12345;
        p[i] = (random_seed >> 16) & 0xFF;
    }
    return size;
}

extern void vga_putchar(char c);
static uint32_t console_write(const void *buf, uint32_t size)
{
    const char *p = (const char *)buf;
    for (uint32_t i = 0; i < size; i++) {
        vga_putchar(p[i]);
    }
    return size;
}

int devfs_register(const char *name, int type, int major, int minor,
                   uint32_t (*read)(void *, uint32_t),
                   uint32_t (*write)(const void *, uint32_t))
{
    if (device_count >= MAX_DEVICES) return -1;
    
    device_node_t *dev = &devices[device_count++];
    strncpy(dev->name, name, 31);
    dev->name[31] = '\0';
    dev->type = type;
    dev->major = major;
    dev->minor = minor;
    dev->read = read;
    dev->write = write;
    
    if (devfs_root) {
        ramfs_create_file(devfs_root, name);
    }
    
    return 0;
}

device_node_t *devfs_get_device(const char *name)
{
    for (int i = 0; i < device_count; i++) {
        if (strcmp(devices[i].name, name) == 0) {
            return &devices[i];
        }
    }
    return NULL;
}

vfs_node_t *devfs_init(vfs_node_t *parent)
{
    devfs_root = ramfs_create_dir(parent, "dev");
    if (!devfs_root) {
        serial_puts("[DEVFS] Failed to create /dev\n");
        return NULL;
    }
    
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    random_seed = lo ^ hi;
    
    /* Register standard character devices */
    devfs_register("null", DEV_TYPE_CHAR, 1, 3, null_read, null_write);
    devfs_register("zero", DEV_TYPE_CHAR, 1, 5, zero_read, zero_write);
    devfs_register("full", DEV_TYPE_CHAR, 1, 7, zero_read, null_write);
    devfs_register("random", DEV_TYPE_CHAR, 1, 8, random_read, null_write);
    devfs_register("urandom", DEV_TYPE_CHAR, 1, 9, random_read, null_write);
    
    /* Console/TTY devices */
    devfs_register("console", DEV_TYPE_CHAR, 5, 1, NULL, console_write);
    devfs_register("tty", DEV_TYPE_CHAR, 5, 0, NULL, console_write);
    devfs_register("tty0", DEV_TYPE_CHAR, 4, 0, NULL, console_write);
    devfs_register("tty1", DEV_TYPE_CHAR, 4, 1, NULL, console_write);
    devfs_register("ttyS0", DEV_TYPE_CHAR, 4, 64, NULL, console_write);  
    
    /* Block devices - hard disks */
    devfs_register("hda", DEV_TYPE_BLOCK, 3, 0, NULL, NULL);
    devfs_register("hda1", DEV_TYPE_BLOCK, 3, 1, NULL, NULL);
    devfs_register("hdb", DEV_TYPE_BLOCK, 3, 64, NULL, NULL);
    devfs_register("sda", DEV_TYPE_BLOCK, 8, 0, NULL, NULL);
    devfs_register("sda1", DEV_TYPE_BLOCK, 8, 1, NULL, NULL);
    
    /* Other devices */
    devfs_register("mem", DEV_TYPE_CHAR, 1, 1, NULL, NULL);
    devfs_register("kmem", DEV_TYPE_CHAR, 1, 2, NULL, NULL);
    devfs_register("fd0", DEV_TYPE_BLOCK, 2, 0, NULL, NULL);  
    
    ramfs_create_dir(devfs_root, "pts");
    
    serial_puts("[DEVFS] Initialized with ");
    char buf[12];
    int i = 0;
    int n = device_count;
    if (n == 0) buf[i++] = '0';
    else { while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; } }
    buf[i] = '\0';
    for (int j = 0; j < i / 2; j++) { char t = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = t; }
    serial_puts(buf);
    serial_puts(" devices\n");
    
    return devfs_root;
}

int devfs_get_count(void)
{
    return device_count;
}

void devfs_list(void)
{
    for (int i = 0; i < device_count; i++) {
        serial_puts("  ");
        serial_puts(devices[i].name);
        serial_puts(" (");
        serial_puts(devices[i].type == DEV_TYPE_CHAR ? "char" : "block");
        serial_puts(")\n");
    }
}

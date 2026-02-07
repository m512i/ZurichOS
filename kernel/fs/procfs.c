/* Process Filesystem (procfs)
 * Virtual filesystem exposing DYNAMIC kernel/process information
 * All data is generated on-read, not stored statically
 */

#include <fs/vfs.h>
#include <fs/ramfs.h>
#include <mm/heap.h>
#include <mm/pmm.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <drivers/pit.h>
#include <drivers/serial.h>
#include <net/net.h>
#include <string.h>

static vfs_node_t *procfs_root = NULL;

#define PROCFS_MEMINFO      1
#define PROCFS_UPTIME       2
#define PROCFS_CPUINFO      3
#define PROCFS_VERSION      4
#define PROCFS_CMDLINE      5
#define PROCFS_FILESYSTEMS  6
#define PROCFS_MOUNTS       7
#define PROCFS_SELF         8
#define PROCFS_LOADAVG      9
#define PROCFS_STAT         10
#define PROCFS_HOSTNAME     11
#define PROCFS_OSTYPE       12
#define PROCFS_OSRELEASE    13
#define PROCFS_KERNELVER    14
#define PROCFS_NET_ARP      15
#define PROCFS_NET_DEV      16
#define PROCFS_NET_ROUTE    17
#define PROCFS_NET_TCP      18
#define PROCFS_NET_UDP      19

typedef struct {
    vfs_node_t vfs;
    int file_type;
} procfs_node_t;

static int uint_to_str(char *buf, uint32_t n)
{
    char tmp[12];
    int i = 0;
    if (n == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }
    while (n > 0) {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    }
    int len = i;
    while (i > 0) {
        buf[len - i] = tmp[i - 1];
        i--;
    }
    buf[len] = '\0';
    return len;
}

static char *str_append(char *dst, const char *src)
{
    while (*src) *dst++ = *src++;
    return dst;
}

static int generate_meminfo(char *buf, uint32_t size)
{
    uint32_t total = pmm_get_total_memory();
    uint32_t free = pmm_get_free_memory();
    uint32_t total_kb = total / 1024;
    uint32_t free_kb = free / 1024;
    uint32_t buffers_kb = 0;
    uint32_t cached_kb = total_kb / 10;
    
    char *p = buf;
    p = str_append(p, "MemTotal:       ");
    p += uint_to_str(p, total_kb);
    p = str_append(p, " kB\n");
    p = str_append(p, "MemFree:        ");
    p += uint_to_str(p, free_kb);
    p = str_append(p, " kB\n");
    p = str_append(p, "MemAvailable:   ");
    p += uint_to_str(p, free_kb + cached_kb);
    p = str_append(p, " kB\n");
    p = str_append(p, "Buffers:        ");
    p += uint_to_str(p, buffers_kb);
    p = str_append(p, " kB\n");
    p = str_append(p, "Cached:         ");
    p += uint_to_str(p, cached_kb);
    p = str_append(p, " kB\n");
    p = str_append(p, "SwapTotal:      0 kB\n");
    p = str_append(p, "SwapFree:       0 kB\n");
    
    (void)size;
    return (int)(p - buf);
}

static int generate_uptime(char *buf, uint32_t size)
{
    uint32_t ticks = pit_get_ticks();
    uint32_t seconds = ticks / 100;  
    uint32_t centiseconds = ticks % 100;
    
    char *p = buf;
    p += uint_to_str(p, seconds);
    *p++ = '.';
    if (centiseconds < 10) *p++ = '0';
    p += uint_to_str(p, centiseconds);
    p = str_append(p, " ");
    p += uint_to_str(p, (seconds * 9) / 10);
    *p++ = '.';
    p += uint_to_str(p, centiseconds);
    *p++ = '\n';
    
    (void)size;
    return (int)(p - buf);
}

static int generate_cpuinfo(char *buf, uint32_t size)
{
    char vendor[13] = {0};
    uint32_t eax, ebx, ecx, edx;
    
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    *(uint32_t *)&vendor[0] = ebx;
    *(uint32_t *)&vendor[4] = edx;
    *(uint32_t *)&vendor[8] = ecx;
    vendor[12] = '\0';
    
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    uint32_t family = (eax >> 8) & 0xF;
    uint32_t model = (eax >> 4) & 0xF;
    uint32_t stepping = eax & 0xF;
    
    char *p = buf;
    p = str_append(p, "processor\t: 0\n");
    p = str_append(p, "vendor_id\t: ");
    p = str_append(p, vendor);
    p = str_append(p, "\n");
    p = str_append(p, "cpu family\t: ");
    p += uint_to_str(p, family);
    p = str_append(p, "\n");
    p = str_append(p, "model\t\t: ");
    p += uint_to_str(p, model);
    p = str_append(p, "\n");
    p = str_append(p, "stepping\t: ");
    p += uint_to_str(p, stepping);
    p = str_append(p, "\n");
    p = str_append(p, "flags\t\t:");
    if (edx & (1 << 0)) p = str_append(p, " fpu");
    if (edx & (1 << 1)) p = str_append(p, " vme");
    if (edx & (1 << 2)) p = str_append(p, " de");
    if (edx & (1 << 3)) p = str_append(p, " pse");
    if (edx & (1 << 4)) p = str_append(p, " tsc");
    if (edx & (1 << 5)) p = str_append(p, " msr");
    if (edx & (1 << 6)) p = str_append(p, " pae");
    if (edx & (1 << 8)) p = str_append(p, " cx8");
    if (edx & (1 << 9)) p = str_append(p, " apic");
    if (edx & (1 << 15)) p = str_append(p, " cmov");
    if (edx & (1 << 19)) p = str_append(p, " clflush");
    if (edx & (1 << 23)) p = str_append(p, " mmx");
    if (edx & (1 << 24)) p = str_append(p, " fxsr");
    if (edx & (1 << 25)) p = str_append(p, " sse");
    if (edx & (1 << 26)) p = str_append(p, " sse2");
    if (ecx & (1 << 0)) p = str_append(p, " sse3");
    if (ecx & (1 << 9)) p = str_append(p, " ssse3");
    if (ecx & (1 << 19)) p = str_append(p, " sse4_1");
    if (ecx & (1 << 20)) p = str_append(p, " sse4_2");
    if (ecx & (1 << 28)) p = str_append(p, " avx");
    p = str_append(p, "\n");
    
    p = str_append(p, "bogomips\t: ");
    p += uint_to_str(p, 4000);  
    p = str_append(p, ".00\n");
    
    (void)size;
    return (int)(p - buf);
}

static int generate_version(char *buf, uint32_t size)
{
    char *p = buf;
    p = str_append(p, "ZurichOS version 0.1.0 (i686-elf-gcc) #1 SMP ");
    p = str_append(p, __DATE__);
    p = str_append(p, " ");
    p = str_append(p, __TIME__);
    p = str_append(p, "\n");
    (void)size;
    return (int)(p - buf);
}

static int generate_cmdline(char *buf, uint32_t size)
{
    char *p = buf;
    p = str_append(p, "root=/dev/hda ro quiet\n");
    (void)size;
    return (int)(p - buf);
}

static int generate_filesystems(char *buf, uint32_t size)
{
    char *p = buf;
    p = str_append(p, "nodev\tramfs\n");
    p = str_append(p, "nodev\tdevfs\n");
    p = str_append(p, "nodev\tprocfs\n");
    p = str_append(p, "\tfat32\n");
    (void)size;
    return (int)(p - buf);
}

static int generate_mounts(char *buf, uint32_t size)
{
    char *p = buf;
    p = str_append(p, "ramfs / ramfs rw 0 0\n");
    p = str_append(p, "devfs /dev devfs rw 0 0\n");
    p = str_append(p, "procfs /proc procfs rw 0 0\n");
    p = str_append(p, "/dev/hda /disks/hda fat32 rw 0 0\n");
    (void)size;
    return (int)(p - buf);
}

static int generate_self(char *buf, uint32_t size)
{
    process_t *proc = process_current();
    uint32_t pid = proc ? proc->pid : 0;
    char *p = buf;
    p += uint_to_str(p, pid);
    *p++ = '\n';
    (void)size;
    return (int)(p - buf);
}

static int generate_loadavg(char *buf, uint32_t size)
{
    extern int scheduler_get_task_count(void);
    int tasks = scheduler_get_task_count();
    
    char *p = buf;
    p = str_append(p, "0.00 0.00 0.00 1/");
    p += uint_to_str(p, tasks > 0 ? tasks : 1);
    p = str_append(p, " 1\n");
    (void)size;
    return (int)(p - buf);
}

static int generate_stat(char *buf, uint32_t size)
{
    uint32_t ticks = pit_get_ticks();
    char *p = buf;
    p = str_append(p, "cpu  ");
    p += uint_to_str(p, ticks / 10);  
    p = str_append(p, " 0 ");
    p += uint_to_str(p, ticks / 100);  
    p = str_append(p, " ");
    p += uint_to_str(p, ticks);  
    p = str_append(p, " 0 0 0 0 0 0\n");
    p = str_append(p, "intr 0\n");
    p = str_append(p, "ctxt 0\n");
    p = str_append(p, "btime 0\n");
    p = str_append(p, "processes 1\n");
    p = str_append(p, "procs_running 1\n");
    p = str_append(p, "procs_blocked 0\n");
    (void)size;
    return (int)(p - buf);
}

static char *format_ip(char *p, uint32_t ip)
{
    p += uint_to_str(p, (ip >> 24) & 0xFF);
    *p++ = '.';
    p += uint_to_str(p, (ip >> 16) & 0xFF);
    *p++ = '.';
    p += uint_to_str(p, (ip >> 8) & 0xFF);
    *p++ = '.';
    p += uint_to_str(p, ip & 0xFF);
    return p;
}

static char *format_mac(char *p, const uint8_t *mac)
{
    const char *hex = "0123456789abcdef";
    for (int i = 0; i < 6; i++) {
        if (i > 0) *p++ = ':';
        *p++ = hex[(mac[i] >> 4) & 0xF];
        *p++ = hex[mac[i] & 0xF];
    }
    return p;
}

static int generate_net_arp(char *buf, uint32_t size)
{
    char *p = buf;
    p = str_append(p, "IP address       HW type  Flags  HW address            Device\n");
    
    extern int arp_get_entry(int index, uint32_t *ip, uint8_t *mac);
    for (int i = 0; i < 32; i++) {
        uint32_t ip = 0;
        uint8_t mac[6] = {0};
        if (arp_get_entry(i, &ip, mac) == 0 && ip != 0) {
            p = format_ip(p, ip);
            int len = 0;
            uint32_t tmp = ip;
            for (int j = 0; j < 4; j++) { int d = (tmp >> (24-j*8)) & 0xFF; len += (d >= 100 ? 3 : d >= 10 ? 2 : 1) + 1; }
            len--;
            while (len < 16) { *p++ = ' '; len++; }
            
            p = str_append(p, " 0x1     0x2    ");
            p = format_mac(p, mac);
            p = str_append(p, "     eth0\n");
        }
    }
    
    (void)size;
    return (int)(p - buf);
}

static int generate_net_dev(char *buf, uint32_t size)
{
    extern netif_t *net_get_default(void);
    netif_t *netif = net_get_default();
    
    char *p = buf;
    p = str_append(p, "Inter-|   Receive                                                |  Transmit\n");
    p = str_append(p, " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n");
    
    p = str_append(p, "    lo:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0\n");
    
    if (netif) {
        p = str_append(p, "  eth0:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0\n");
    }
    
    (void)size;
    return (int)(p - buf);
}

static int generate_net_route(char *buf, uint32_t size)
{
    extern netif_t *net_get_default(void);
    netif_t *netif = net_get_default();
    
    char *p = buf;
    p = str_append(p, "Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT\n");
    
    if (netif && netif->ip != 0) {
        p = str_append(p, "eth0\t00000000\t");
        uint32_t gw = netif->gateway;
        char hex[9];
        const char *hexc = "0123456789ABCDEF";
        for (int i = 0; i < 8; i++) hex[i] = hexc[(gw >> ((7-i)*4)) & 0xF];
        hex[8] = '\0';
        p = str_append(p, hex);
        p = str_append(p, "\t0003\t0\t0\t0\t00000000\t0\t0\t0\n");
        
        uint32_t net = netif->ip & netif->netmask;
        p = str_append(p, "eth0\t");
        for (int i = 0; i < 8; i++) hex[i] = hexc[(net >> ((7-i)*4)) & 0xF];
        p = str_append(p, hex);
        p = str_append(p, "\t00000000\t0001\t0\t0\t0\t");
        uint32_t mask = netif->netmask;
        for (int i = 0; i < 8; i++) hex[i] = hexc[(mask >> ((7-i)*4)) & 0xF];
        p = str_append(p, hex);
        p = str_append(p, "\t0\t0\t0\n");
    }
    
    (void)size;
    return (int)(p - buf);
}

static int generate_net_tcp(char *buf, uint32_t size)
{
    extern int socket_get_info(int index, int *type, uint32_t *local_ip, uint16_t *local_port,
                               uint32_t *remote_ip, uint16_t *remote_port, int *state);
    
    char *p = buf;
    p = str_append(p, "  sl  local_address rem_address   st tx_queue rx_queue\n");
    
    int slot = 0;
    for (int i = 0; i < 64; i++) {
        int type, state;
        uint32_t local_ip, remote_ip;
        uint16_t local_port, remote_port;
        
        if (socket_get_info(i, &type, &local_ip, &local_port, &remote_ip, &remote_port, &state) == 0) {
            if (type == 1) {  
                *p++ = ' ';
                if (slot < 10) *p++ = ' ';
                p += uint_to_str(p, slot);
                p = str_append(p, ": ");
                
                char hex[9];
                const char *hexc = "0123456789ABCDEF";
                for (int j = 0; j < 8; j++) hex[j] = hexc[(local_ip >> ((7-j)*4)) & 0xF];
                hex[8] = '\0';
                p = str_append(p, hex);
                *p++ = ':';
                for (int j = 0; j < 4; j++) hex[j] = hexc[(local_port >> ((3-j)*4)) & 0xF];
                hex[4] = '\0';
                p = str_append(p, hex);
                *p++ = ' ';
                
                for (int j = 0; j < 8; j++) hex[j] = hexc[(remote_ip >> ((7-j)*4)) & 0xF];
                hex[8] = '\0';
                p = str_append(p, hex);
                *p++ = ':';
                for (int j = 0; j < 4; j++) hex[j] = hexc[(remote_port >> ((3-j)*4)) & 0xF];
                hex[4] = '\0';
                p = str_append(p, hex);
                
                p = str_append(p, " ");
                if (state == 1) p = str_append(p, "0A");  
                else if (state == 2) p = str_append(p, "01");  
                else p = str_append(p, "07");  
                
                p = str_append(p, " 00000000:00000000 00000000:00000000\n");
                slot++;
            }
        }
    }
    
    (void)size;
    return (int)(p - buf);
}

static int generate_net_udp(char *buf, uint32_t size)
{
    extern int socket_get_info(int index, int *type, uint32_t *local_ip, uint16_t *local_port,
                               uint32_t *remote_ip, uint16_t *remote_port, int *state);
    
    char *p = buf;
    p = str_append(p, "  sl  local_address rem_address   st tx_queue rx_queue\n");
    
    int slot = 0;
    for (int i = 0; i < 64; i++) {
        int type, state;
        uint32_t local_ip, remote_ip;
        uint16_t local_port, remote_port;
        
        if (socket_get_info(i, &type, &local_ip, &local_port, &remote_ip, &remote_port, &state) == 0) {
            if (type == 2) {  
                *p++ = ' ';
                if (slot < 10) *p++ = ' ';
                p += uint_to_str(p, slot);
                p = str_append(p, ": ");
                
                char hex[9];
                const char *hexc = "0123456789ABCDEF";
                for (int j = 0; j < 8; j++) hex[j] = hexc[(local_ip >> ((7-j)*4)) & 0xF];
                hex[8] = '\0';
                p = str_append(p, hex);
                *p++ = ':';
                for (int j = 0; j < 4; j++) hex[j] = hexc[(local_port >> ((3-j)*4)) & 0xF];
                hex[4] = '\0';
                p = str_append(p, hex);
                *p++ = ' ';
                
                for (int j = 0; j < 8; j++) hex[j] = hexc[(remote_ip >> ((7-j)*4)) & 0xF];
                hex[8] = '\0';
                p = str_append(p, hex);
                *p++ = ':';
                for (int j = 0; j < 4; j++) hex[j] = hexc[(remote_port >> ((3-j)*4)) & 0xF];
                hex[4] = '\0';
                p = str_append(p, hex);
                
                p = str_append(p, " 07 00000000:00000000 00000000:00000000\n");
                slot++;
            }
        }
    }
    
    (void)size;
    return (int)(p - buf);
}

static char procfs_buffer[2048];

static int procfs_dynamic_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    procfs_node_t *pnode = (procfs_node_t *)node->impl;
    if (!pnode) return -1;
    
    int len = 0;
    switch (pnode->file_type) {
        case PROCFS_MEMINFO:
            len = generate_meminfo(procfs_buffer, sizeof(procfs_buffer));
            break;
        case PROCFS_UPTIME:
            len = generate_uptime(procfs_buffer, sizeof(procfs_buffer));
            break;
        case PROCFS_CPUINFO:
            len = generate_cpuinfo(procfs_buffer, sizeof(procfs_buffer));
            break;
        case PROCFS_VERSION:
            len = generate_version(procfs_buffer, sizeof(procfs_buffer));
            break;
        case PROCFS_CMDLINE:
            len = generate_cmdline(procfs_buffer, sizeof(procfs_buffer));
            break;
        case PROCFS_FILESYSTEMS:
            len = generate_filesystems(procfs_buffer, sizeof(procfs_buffer));
            break;
        case PROCFS_MOUNTS:
            len = generate_mounts(procfs_buffer, sizeof(procfs_buffer));
            break;
        case PROCFS_SELF:
            len = generate_self(procfs_buffer, sizeof(procfs_buffer));
            break;
        case PROCFS_LOADAVG:
            len = generate_loadavg(procfs_buffer, sizeof(procfs_buffer));
            break;
        case PROCFS_STAT:
            len = generate_stat(procfs_buffer, sizeof(procfs_buffer));
            break;
        case PROCFS_NET_ARP:
            len = generate_net_arp(procfs_buffer, sizeof(procfs_buffer));
            break;
        case PROCFS_NET_DEV:
            len = generate_net_dev(procfs_buffer, sizeof(procfs_buffer));
            break;
        case PROCFS_NET_ROUTE:
            len = generate_net_route(procfs_buffer, sizeof(procfs_buffer));
            break;
        case PROCFS_NET_TCP:
            len = generate_net_tcp(procfs_buffer, sizeof(procfs_buffer));
            break;
        case PROCFS_NET_UDP:
            len = generate_net_udp(procfs_buffer, sizeof(procfs_buffer));
            break;
        case PROCFS_HOSTNAME: {
            char *p = procfs_buffer;
            p = str_append(p, "zurich\n");
            len = (int)(p - procfs_buffer);
            break;
        }
        case PROCFS_OSTYPE: {
            char *p = procfs_buffer;
            p = str_append(p, "ZurichOS\n");
            len = (int)(p - procfs_buffer);
            break;
        }
        case PROCFS_OSRELEASE: {
            char *p = procfs_buffer;
            p = str_append(p, "0.1.0\n");
            len = (int)(p - procfs_buffer);
            break;
        }
        case PROCFS_KERNELVER: {
            char *p = procfs_buffer;
            p = str_append(p, "#1 SMP ");
            p = str_append(p, __DATE__);
            p = str_append(p, "\n");
            len = (int)(p - procfs_buffer);
            break;
        }
        default:
            return 0;
    }
    
    if ((int)offset >= len) return 0;
    
    int bytes_to_copy = len - offset;
    if (bytes_to_copy > (int)size) bytes_to_copy = size;
    
    memcpy(buffer, procfs_buffer + offset, bytes_to_copy);
    return bytes_to_copy;
}

static vfs_node_t *procfs_create_dynamic(vfs_node_t *parent, const char *name, int file_type)
{
    procfs_node_t *node = kmalloc(sizeof(procfs_node_t));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(procfs_node_t));
    strncpy(node->vfs.name, name, VFS_MAX_NAME - 1);
    node->vfs.flags = VFS_FILE;
    node->vfs.read = procfs_dynamic_read;
    node->vfs.impl = node;
    node->file_type = file_type;
    
    int len = 0;
    switch (file_type) {
        case PROCFS_MEMINFO: len = 300; break;
        case PROCFS_UPTIME: len = 32; break;
        case PROCFS_CPUINFO: len = 400; break;
        case PROCFS_VERSION: len = 100; break;
        case PROCFS_CMDLINE: len = 32; break;
        case PROCFS_FILESYSTEMS: len = 64; break;
        case PROCFS_MOUNTS: len = 200; break;
        case PROCFS_SELF: len = 8; break;
        case PROCFS_LOADAVG: len = 32; break;
        case PROCFS_STAT: len = 256; break;
        case PROCFS_NET_ARP: len = 256; break;
        case PROCFS_NET_DEV: len = 512; break;
        case PROCFS_NET_ROUTE: len = 256; break;
        case PROCFS_NET_TCP: len = 256; break;
        case PROCFS_NET_UDP: len = 256; break;
        case PROCFS_HOSTNAME: len = 7; break;
        case PROCFS_OSTYPE: len = 9; break;
        case PROCFS_OSRELEASE: len = 6; break;
        case PROCFS_KERNELVER: len = 20; break;
        default: len = 64; break;
    }
    node->vfs.length = len;
    
    vfs_node_t *file = ramfs_create_file(parent, name);
    if (file) {
        file->read = procfs_dynamic_read;
        file->impl = node;
        file->length = len;
    }
    
    return file;
}

vfs_node_t *procfs_init(vfs_node_t *parent)
{
    procfs_root = ramfs_create_dir(parent, "proc");
    if (!procfs_root) {
        serial_puts("[PROCFS] Failed to create /proc\n");
        return NULL;
    }
    
    procfs_create_dynamic(procfs_root, "meminfo", PROCFS_MEMINFO);
    procfs_create_dynamic(procfs_root, "uptime", PROCFS_UPTIME);
    procfs_create_dynamic(procfs_root, "cpuinfo", PROCFS_CPUINFO);
    procfs_create_dynamic(procfs_root, "version", PROCFS_VERSION);
    procfs_create_dynamic(procfs_root, "cmdline", PROCFS_CMDLINE);
    procfs_create_dynamic(procfs_root, "filesystems", PROCFS_FILESYSTEMS);
    procfs_create_dynamic(procfs_root, "mounts", PROCFS_MOUNTS);
    procfs_create_dynamic(procfs_root, "self", PROCFS_SELF);
    procfs_create_dynamic(procfs_root, "loadavg", PROCFS_LOADAVG);
    procfs_create_dynamic(procfs_root, "stat", PROCFS_STAT);
    
    /* Create /proc/sys directory */
    vfs_node_t *sys_dir = ramfs_create_dir(procfs_root, "sys");
    if (sys_dir) {
        vfs_node_t *kernel_dir = ramfs_create_dir(sys_dir, "kernel");
        if (kernel_dir) {
            procfs_create_dynamic(kernel_dir, "hostname", PROCFS_HOSTNAME);
            procfs_create_dynamic(kernel_dir, "ostype", PROCFS_OSTYPE);
            procfs_create_dynamic(kernel_dir, "osrelease", PROCFS_OSRELEASE);
            procfs_create_dynamic(kernel_dir, "version", PROCFS_KERNELVER);
        }
        
        /* /proc/sys/net */
        vfs_node_t *net_dir = ramfs_create_dir(sys_dir, "net");
        if (net_dir) {
            vfs_node_t *ipv4_dir = ramfs_create_dir(net_dir, "ipv4");
            if (ipv4_dir) {
                vfs_node_t *f = ramfs_create_file(ipv4_dir, "ip_forward");
                if (f) ramfs_write_file(f, "0\n", 2);
                f = ramfs_create_file(ipv4_dir, "tcp_syncookies");
                if (f) ramfs_write_file(f, "1\n", 2);
            }
        }
        
        /* /proc/sys/vm */
        vfs_node_t *vm_dir = ramfs_create_dir(sys_dir, "vm");
        if (vm_dir) {
            vfs_node_t *f = ramfs_create_file(vm_dir, "swappiness");
            if (f) ramfs_write_file(f, "60\n", 3);
        }
    }
    
    vfs_node_t *proc_net_dir = ramfs_create_dir(procfs_root, "net");
    if (proc_net_dir) {
        procfs_create_dynamic(proc_net_dir, "arp", PROCFS_NET_ARP);
        procfs_create_dynamic(proc_net_dir, "dev", PROCFS_NET_DEV);
        procfs_create_dynamic(proc_net_dir, "route", PROCFS_NET_ROUTE);
        procfs_create_dynamic(proc_net_dir, "tcp", PROCFS_NET_TCP);
        procfs_create_dynamic(proc_net_dir, "udp", PROCFS_NET_UDP);
        vfs_node_t *f = ramfs_create_file(proc_net_dir, "unix");
        if (f) ramfs_write_file(f, "Num       RefCount Protocol Flags    Type St Path\n", 50);
    }
    
    vfs_node_t *fs_dir = ramfs_create_dir(procfs_root, "fs");
    if (fs_dir) {
        vfs_node_t *f = ramfs_create_file(fs_dir, "file-nr");
        if (f) ramfs_write_file(f, "0\t0\t8192\n", 9);
        f = ramfs_create_file(fs_dir, "inode-nr");
        if (f) ramfs_write_file(f, "0\t0\n", 4);
    }
    
    serial_puts("[PROCFS] Initialized with dynamic content\n");
    return procfs_root;
}

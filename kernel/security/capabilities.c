/* Capabilities - Fine-grained Privilege Management
 * Linux-style capability system
 */

#include <security/security.h>
#include <kernel/kernel.h>
#include <drivers/serial.h>
#include <string.h>

#define MAX_PROCESSES 256

typedef struct {
    uint64_t effective;
    uint64_t permitted;
    uint64_t inheritable;
} process_caps_t;

static process_caps_t process_capabilities[MAX_PROCESSES];

void capabilities_init(void)
{
    memset(process_capabilities, 0, sizeof(process_capabilities));
    
    process_capabilities[0].effective = CAP_ALL;
    process_capabilities[0].permitted = CAP_ALL;
    process_capabilities[0].inheritable = CAP_ALL;
    
    process_capabilities[1].effective = CAP_ALL;
    process_capabilities[1].permitted = CAP_ALL;
    process_capabilities[1].inheritable = CAP_ALL;
    
    serial_puts("[CAPS] Capabilities initialized\n");
}

uint64_t capability_get(uint32_t pid)
{
    if (pid >= MAX_PROCESSES) return 0;
    return process_capabilities[pid].effective;
}

int capability_set(uint32_t pid, uint64_t caps)
{
    if (pid >= MAX_PROCESSES) return -1;
    
    process_capabilities[pid].effective = caps & process_capabilities[pid].permitted;
    return 0;
}

int capability_has(uint32_t pid, uint64_t cap)
{
    if (pid >= MAX_PROCESSES) return 0;
    return (process_capabilities[pid].effective & cap) == cap;
}

int capability_drop(uint32_t pid, uint64_t cap)
{
    if (pid >= MAX_PROCESSES) return -1;
    
    process_capabilities[pid].effective &= ~cap;
    process_capabilities[pid].permitted &= ~cap;
    
    return 0;
}

int capability_grant(uint32_t pid, uint64_t cap)
{
    if (pid >= MAX_PROCESSES) return -1;
    
    process_capabilities[pid].permitted |= cap;
    process_capabilities[pid].effective |= cap;
    
    return 0;
}

void capability_inherit(uint32_t parent_pid, uint32_t child_pid)
{
    if (parent_pid >= MAX_PROCESSES || child_pid >= MAX_PROCESSES) return;
    
    process_capabilities[child_pid].permitted = process_capabilities[parent_pid].inheritable;
    process_capabilities[child_pid].effective = process_capabilities[parent_pid].inheritable;
    process_capabilities[child_pid].inheritable = process_capabilities[parent_pid].inheritable;
}

void capability_clear(uint32_t pid)
{
    if (pid >= MAX_PROCESSES) return;
    
    process_capabilities[pid].effective = 0;
    process_capabilities[pid].permitted = 0;
    process_capabilities[pid].inheritable = 0;
}

int capability_check(uint32_t pid, uint64_t required_cap, const char *operation)
{
    if (capability_has(pid, required_cap)) {
        return 0;
    }
    
    serial_puts("[CAPS] Permission denied: ");
    serial_puts(operation);
    serial_puts(" for PID ");
    char buf[12];
    int i = 0;
    uint32_t n = pid;
    if (n == 0) buf[i++] = '0';
    else { while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; } }
    buf[i] = '\0';
    for (int j = 0; j < i / 2; j++) { char t = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = t; }
    serial_puts(buf);
    serial_puts("\n");
    
    return -1;
}

const char *capability_name(uint64_t cap)
{
    if (cap == CAP_CHOWN) return "CAP_CHOWN";
    if (cap == CAP_DAC_OVERRIDE) return "CAP_DAC_OVERRIDE";
    if (cap == CAP_KILL) return "CAP_KILL";
    if (cap == CAP_SETUID) return "CAP_SETUID";
    if (cap == CAP_SETGID) return "CAP_SETGID";
    if (cap == CAP_NET_BIND) return "CAP_NET_BIND";
    if (cap == CAP_NET_RAW) return "CAP_NET_RAW";
    if (cap == CAP_SYS_BOOT) return "CAP_SYS_BOOT";
    if (cap == CAP_SYS_MODULE) return "CAP_SYS_MODULE";
    if (cap == CAP_SYS_ADMIN) return "CAP_SYS_ADMIN";
    if (cap == CAP_SYS_PTRACE) return "CAP_SYS_PTRACE";
    if (cap == CAP_MKNOD) return "CAP_MKNOD";
    return "UNKNOWN";
}

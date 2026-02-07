/* Seccomp - Secure Computing Mode
 * Syscall filtering for sandboxing
 */

#include <security/security.h>
#include <kernel/kernel.h>
#include <drivers/serial.h>
#include <string.h>

#define MAX_PROCESSES 256
static seccomp_filter_t process_filters[MAX_PROCESSES];

#define SYS_READ    1
#define SYS_WRITE   2
#define SYS_EXIT    0
#define SYS_SIGRETURN 119

void seccomp_init(void)
{
    memset(process_filters, 0, sizeof(process_filters));
    serial_puts("[SECCOMP] Initialized\n");
}

int seccomp_set_mode(uint32_t pid, int mode)
{
    if (pid >= MAX_PROCESSES) return -1;
    
    seccomp_filter_t *filter = &process_filters[pid];
    
    if (mode == SECCOMP_MODE_STRICT) {
        filter->mode = SECCOMP_MODE_STRICT;
        filter->rule_count = 0;
        
        filter->rules[filter->rule_count].syscall_nr = SYS_READ;
        filter->rules[filter->rule_count].action = SECCOMP_RET_ALLOW;
        filter->rule_count++;
        
        filter->rules[filter->rule_count].syscall_nr = SYS_WRITE;
        filter->rules[filter->rule_count].action = SECCOMP_RET_ALLOW;
        filter->rule_count++;
        
        filter->rules[filter->rule_count].syscall_nr = SYS_EXIT;
        filter->rules[filter->rule_count].action = SECCOMP_RET_ALLOW;
        filter->rule_count++;
        
        filter->rules[filter->rule_count].syscall_nr = SYS_SIGRETURN;
        filter->rules[filter->rule_count].action = SECCOMP_RET_ALLOW;
        filter->rule_count++;
        
        serial_puts("[SECCOMP] Strict mode enabled for PID ");
        char buf[12];
        int i = 0;
        uint32_t n = pid;
        if (n == 0) buf[i++] = '0';
        else { while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; } }
        buf[i] = '\0';
        for (int j = 0; j < i / 2; j++) { char t = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = t; }
        serial_puts(buf);
        serial_puts("\n");
    } else if (mode == SECCOMP_MODE_FILTER) {
        filter->mode = SECCOMP_MODE_FILTER;
    } else if (mode == SECCOMP_MODE_DISABLED) {
        filter->mode = SECCOMP_MODE_DISABLED;
        filter->rule_count = 0;
    } else {
        return -1;
    }
    
    return 0;
}

int seccomp_add_rule(uint32_t pid, uint32_t syscall_nr, uint32_t action)
{
    if (pid >= MAX_PROCESSES) return -1;
    
    seccomp_filter_t *filter = &process_filters[pid];
    
    if (filter->mode != SECCOMP_MODE_FILTER) return -1;
    if (filter->rule_count >= MAX_SECCOMP_RULES) return -2;
    
    filter->rules[filter->rule_count].syscall_nr = syscall_nr;
    filter->rules[filter->rule_count].action = action;
    filter->rule_count++;
    
    return 0;
}

int seccomp_check(uint32_t pid, uint32_t syscall_nr)
{
    if (pid >= MAX_PROCESSES) return SECCOMP_RET_ALLOW;
    
    seccomp_filter_t *filter = &process_filters[pid];
    
    if (filter->mode == SECCOMP_MODE_DISABLED) {
        return SECCOMP_RET_ALLOW;
    }
    
    for (int i = 0; i < filter->rule_count; i++) {
        if (filter->rules[i].syscall_nr == syscall_nr) {
            return filter->rules[i].action;
        }
    }
    
    if (filter->mode == SECCOMP_MODE_STRICT) {
        return SECCOMP_RET_KILL;  
    }
    
    return SECCOMP_RET_ALLOW;
}

int seccomp_get_mode(uint32_t pid)
{
    if (pid >= MAX_PROCESSES) return SECCOMP_MODE_DISABLED;
    return process_filters[pid].mode;
}

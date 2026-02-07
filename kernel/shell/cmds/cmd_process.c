/* Shell Commands - Process Category
 * ps, kill, tasks, exec
 */

#include <shell/builtins.h>
#include <kernel/shell.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/elf.h>
#include <drivers/vga.h>
#include <fs/vfs.h>
#include <string.h>

void cmd_ps(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("  PID  PPID  STATE     NAME\n");
    vga_puts("  ---  ----  -------   ----\n");
    
    uint32_t index = 0;
    process_t *proc;
    
    while ((proc = process_iterate(&index)) != NULL) {
        vga_puts("  ");
        if (proc->pid < 10) vga_puts(" ");
        vga_put_dec(proc->pid);
        
        vga_puts("   ");
        if (proc->ppid < 10) vga_puts(" ");
        vga_put_dec(proc->ppid);
        
        vga_puts("   ");
        const char *state = process_state_name(proc->state);
        vga_puts(state);
        for (int i = strlen(state); i < 10; i++) {
            vga_putchar(' ');
        }
        
        vga_puts(proc->name);
        vga_puts("\n");
    }
    
    vga_puts("\nTotal: ");
    vga_put_dec(process_count());
    vga_puts(" process(es)\n");
}

void cmd_kill(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: kill <pid|name>\n");
        return;
    }
    
    uint32_t pid;
    
    if (argv[1][0] >= '0' && argv[1][0] <= '9') {
        pid = shell_parse_dec(argv[1]);
    } else {
        uint32_t index = 0;
        process_t *proc;
        pid = (uint32_t)-1;  
        
        while ((proc = process_iterate(&index)) != NULL) {
            if (strcmp(proc->name, argv[1]) == 0) {
                pid = proc->pid;
                break;
            }
        }
        
        if (pid == (uint32_t)-1) {
            vga_puts("kill: no process named: ");
            vga_puts(argv[1]);
            vga_puts("\n");
            return;
        }
    }
    
    int result = process_kill(pid);
    
    switch (result) {
        case 0:
            vga_puts("Process ");
            vga_put_dec(pid);
            vga_puts(" killed\n");
            break;
        case -1:
            vga_puts("kill: cannot kill kernel (pid 0)\n");
            break;
        case -2:
            vga_puts("kill: cannot kill shell (pid 1)\n");
            break;
        case -3:
            vga_puts("kill: no such process: ");
            vga_put_dec(pid);
            vga_puts("\n");
            break;
        default:
            vga_puts("kill: failed\n");
    }
}

void cmd_tasks(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("Scheduler Tasks:\n");
    vga_puts("----------------\n");
    vga_puts("TID  NAME             STATE      CPU TIME\n");
    
    task_t *current = task_current();
    
    if (current) {
        vga_put_dec(current->tid);
        if (current->tid < 10) vga_puts("    ");
        else if (current->tid < 100) vga_puts("   ");
        else vga_puts("  ");
        
        vga_puts(current->name);
        int len = strlen(current->name);
        for (int i = len; i < 17; i++) vga_putchar(' ');
        
        switch (current->state) {
            case TASK_STATE_RUNNING:  vga_puts("running    "); break;
            case TASK_STATE_READY:    vga_puts("ready      "); break;
            case TASK_STATE_BLOCKED:  vga_puts("blocked    "); break;
            case TASK_STATE_SLEEPING: vga_puts("sleeping   "); break;
            case TASK_STATE_ZOMBIE:   vga_puts("zombie     "); break;
            default:                  vga_puts("unknown    "); break;
        }
        
        vga_put_dec((uint32_t)current->cpu_time);
        vga_puts(" ticks\n");
    }
    
    vga_puts("\n(Full task list requires scheduler iteration API)\n");
}

void cmd_exec(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: exec <program>\n");
        return;
    }
    
    vga_puts("Root directory contents:\n");
    dirent_t *dir;
    for (uint32_t i = 0; (dir = vfs_readdir(vfs_get_root(), i)) != NULL; i++) {
        vga_puts("  ");
        vga_puts(dir->name);
        if (vfs_is_directory(vfs_finddir(vfs_get_root(), dir->name))) {
            vga_puts(" (dir)");
        }
        vga_puts("\n");
    }
    vga_puts("\n");
    
    char upper_name[64];
    int j = 0;
    for (int i = 0; argv[1][i] && j < 63; i++) {
        if (argv[1][i] >= 'a' && argv[1][i] <= 'z') {
            upper_name[j++] = argv[1][i] - 32;
        } else {
            upper_name[j++] = argv[1][i];
        }
    }
    upper_name[j] = '\0';
    
    user_process_t *proc = elf_load_from_file(argv[1]);
    
    if (!proc) {
        proc = elf_load_from_file(upper_name);
    }
    
    if (!proc) {
        char path[256];
        strcpy(path, "/disks/hda/");
        strcat(path, argv[1]);
        proc = elf_load_from_file(path);
    }
    
    if (!proc) {
        char path[256];
        strcpy(path, "/disks/hda/");
        strcat(path, upper_name);
        proc = elf_load_from_file(path);
    }
    
    if (!proc) {
        vga_puts("Failed to load program: ");
        vga_puts(argv[1]);
        vga_puts("\n");
        vga_puts("Note: Copy ");
        vga_puts(argv[1]);
        vga_puts(" to FAT32 disk first\n");
        return;
    }
    
    vga_puts("Executing: ");
    vga_puts(argv[1]);
    vga_puts("\n");
    
    elf_execute(proc);
}

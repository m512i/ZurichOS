/* Shell Command Table and Help
 * Command implementations are in category-based files:
 *   cmd_general.c, cmd_fs.c, cmd_mem.c, cmd_disk.c,
 *   cmd_process.c, cmd_system.c, cmd_debug.c
 */

#include <shell/builtins.h>
#include <shell/shell_features.h>
#include <kernel/shell.h>
#include <drivers/vga.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *description;
} command_category_t;

static const command_category_t categories[] = {
    {"general", "General commands (help, clear, echo, version, etc.)"},
    {"fs",      "Filesystem commands (ls, cd, cat, mkdir, etc.)"},
    {"mem",     "Memory commands (mem, hexdump, peek, poke, etc.)"},
    {"disk",    "Disk commands (lsblk, hdinfo, fatmount, etc.)"},
    {"process", "Process commands (ps, kill, tasks, exec)"},
    {"system",  "System commands (time, date, lspci, apic, etc.)"},
    {"debug",   "Debug commands (panic, vga, beep, synctest, etc.)"},
    {"net",     "Network commands (netinit, ifconfig, ping, etc.)"},
    {"security", "Security commands (users, groups, aslr, etc.)"},
    {"shell",    "Shell builtins (export, env, jobs, history, etc.)"},
    {"utils",    "Core utilities (grep, find, wc, head, tail, sort, etc.)"},
    {NULL, NULL}
};

static shell_command_t general_commands[] = {
    {"help",     "Show available commands",           cmd_help},
    {"clear",    "Clear the screen",                  cmd_clear},
    {"echo",     "Print text to screen",              cmd_echo},
    {"version",  "Show OS version",                   cmd_version},
    {"uptime",   "Show system uptime",                cmd_uptime},
    {"color",    "Set text color (0-15)",             cmd_color},
    {"exit",     "Exit shell (same as halt)",         cmd_exit},
    {"halt",     "Halt the system",                   cmd_halt},
    {"reboot",   "Reboot the system",                 cmd_reboot},
    {NULL, NULL, NULL}
};

static shell_command_t fs_commands[] = {
    {"ls",       "List directory contents",           cmd_ls},
    {"cd",       "Change directory: cd <path>",       cmd_cd},
    {"pwd",      "Print working directory",           cmd_pwd},
    {"cat",      "Display file contents: cat <file>", cmd_cat},
    {"touch",    "Create empty file: touch <name>",   cmd_touch},
    {"mkdir",    "Create directory: mkdir <name>",    cmd_mkdir},
    {"rmdir",    "Remove empty directory: rmdir <name>", cmd_rmdir},
    {"rm",       "Remove file: rm <name>",            cmd_rm},
    {"write",    "Write to file: write <file> <text>",cmd_write},
    {"append",   "Append to file: append <file> <text>", cmd_append},
    {"cp",       "Copy file: cp <src> <dest>",        cmd_cp},
    {"mv",       "Move/rename: mv <src> <dest>",      cmd_mv},
    {"stat",     "Show file info: stat <name>",       cmd_stat},
    {"tree",     "Show directory tree: tree [path]",  cmd_tree},
    {NULL, NULL, NULL}
};

static shell_command_t mem_commands[] = {
    {"mem",       "Show memory information",           cmd_mem},
    {"free",      "Show memory usage (Linux-style)",   cmd_free},
    {"hexdump",   "Dump memory: hexdump <addr> [len]", cmd_hexdump},
    {"peek",      "Read memory: peek <addr>",          cmd_peek},
    {"poke",      "Write memory: poke <addr> <val>",   cmd_poke},
    {"alloc",     "Allocate memory: alloc <size>",     cmd_alloc},
    {"memtest",   "Test memory allocation/mapping",    cmd_memtest},
    {"heapstats", "Show heap allocation statistics",   cmd_heapstats},
    {"leaktest",  "Test memory leak detection",        cmd_leaktest},
    {NULL, NULL, NULL}
};

static shell_command_t disk_commands[] = {
    {"lsblk",    "List block devices (disks)",        cmd_lsblk},
    {"hdinfo",   "Show disk info: hdinfo <drive>",    cmd_hdinfo},
    {"readsec",  "Read sector: readsec <drv> <lba>",  cmd_readsec},
    {"fatmount", "Mount FAT32: fatmount <drive>",     cmd_fatmount},
    {"fatls",    "List FAT32 dir: fatls [path]",      cmd_fatls},
    {"fatcat",   "Read FAT32 file: fatcat <file>",    cmd_fatcat},
    {"mounts",   "Show mounted filesystems",          cmd_mounts},
    {NULL, NULL, NULL}
};

static shell_command_t process_commands[] = {
    {"ps",       "List running processes",            cmd_ps},
    {"kill",     "Kill a process: kill <pid>",        cmd_kill},
    {"tasks",    "List scheduler tasks",              cmd_tasks},
    {"exec",     "Execute user program",              cmd_exec},
    {NULL, NULL, NULL}
};

static shell_command_t system_commands[] = {
    {"time",     "Show current time",                 cmd_time},
    {"date",     "Show current date",                 cmd_date},
    {"timezone", "Set timezone: timezone <offset>",   cmd_timezone},
    {"lspci",    "List PCI devices",                  cmd_lspci},
    {"apic",     "Show APIC status and info",         cmd_apic},
    {"drivers",  "List registered PCI drivers",       cmd_drivers},
    {"symbols",  "Show kernel symbol addresses",      cmd_symbols},
    {"isolation","Driver isolation status",            cmd_isolation},
    {NULL, NULL, NULL}
};

static shell_command_t debug_commands[] = {
    {"panic",      "Test kernel panic handler",         cmd_panic},
    {"vga",        "Write to VGA buffer directly",      cmd_vga},
    {"beep",       "Play a beep: beep [freq] [ms]",     cmd_beep},
    {"play",       "Play a tune",                       cmd_play},
    {"synctest",   "Test mutex/semaphore blocking",     cmd_synctest},
    {"pritest",    "Test priority inheritance",         cmd_pritest},
    {"cvtest",     "Test condition variables",          cmd_cvtest},
    {"rwtest",     "Test read-write locks",             cmd_rwtest},
    {"asserttest", "Test ASSERT macro (will halt)",     cmd_asserttest},
    {"guardtest",  "Test memory guard detection",       cmd_guardtest},
    {NULL, NULL, NULL}
};

static shell_command_t net_commands[] = {
    {"netinit",    "Initialize network stack",          cmd_netinit},
    {"ifconfig",   "Show/set IP: ifconfig [ip mask gw]",cmd_ifconfig},
    {"ping",       "Send ICMP ping: ping <ip>",         cmd_ping},
    {"arp",        "Show ARP cache",                    cmd_arp},
    {"netpoll",    "Poll network for packets",          cmd_netpoll},
    {"netstat",    "Show active connections",           cmd_netstat},
    {"dhcp",       "Get IP via DHCP",                   cmd_dhcp},
    {"dns",        "Resolve hostname: dns <host>",      cmd_dns},
    {"route",      "Show routing table",                cmd_route},
    {NULL, NULL, NULL}
};

static shell_command_t security_commands[] = {
    {"security",   "Show security status",              cmd_security},
    {"users",      "List user accounts",                cmd_users},
    {"groups",     "List groups",                       cmd_groups},
    {"useradd",    "Add user: useradd <name> <uid> <gid>", cmd_useradd},
    {"groupadd",   "Add group: groupadd <name> <gid>",  cmd_groupadd},
    {"aslr",       "ASLR control: aslr <on|off>",       cmd_aslr},
    {"whoami",     "Show current user",                 cmd_whoami},
    {"id",         "Show user/group IDs",               cmd_id},
    {NULL, NULL, NULL}
};

static shell_command_t utils_commands[] = {
    {"grep",     "Pattern match: grep [-inc] <pat> <file>", cmd_grep},
    {"find",     "File search: find [path] [-name pat] [-type f|d]", cmd_find},
    {"wc",       "Word/line count: wc [-lwc] <file>",      cmd_wc},
    {"head",     "First lines: head [-n N] <file>",        cmd_head},
    {"tail",     "Last lines: tail [-n N] <file>",         cmd_tail},
    {"sort",     "Sort lines: sort [-r] <file>",           cmd_sort},
    {"uniq",     "Remove dupes: uniq [-cd] <file>",        cmd_uniq},
    {"diff",     "Compare files: diff <f1> <f2>",          cmd_diff},
    {"tar",      "Archive: tar <list|create|extract> <arc> [files]", cmd_tar},
    {NULL, NULL, NULL}
};

static shell_command_t shell_builtins[] = {
    {"export",     "Set/show env vars: export VAR=val",  cmd_export},
    {"unset",      "Remove env variable: unset VAR",     cmd_unset},
    {"env",        "Show all environment variables",     cmd_env},
    {"set",        "Set variable: set VAR value",        cmd_set},
    {"source",     "Run script: source <file>",          cmd_source},
    {"jobs",       "List background jobs",               cmd_jobs},
    {"fg",         "Bring job to foreground",            cmd_fg},
    {"history",    "Show command history",               cmd_history},
    {"alias",      "Define alias: alias name=value",     cmd_alias},
    {NULL, NULL, NULL}
};

shell_command_t shell_commands[] = {
    /* General */
    {"help",     "Show available commands",           cmd_help},
    {"clear",    "Clear the screen",                  cmd_clear},
    {"echo",     "Print text to screen",              cmd_echo},
    {"version",  "Show OS version",                   cmd_version},
    {"uptime",   "Show system uptime",                cmd_uptime},
    {"color",    "Set text color (0-15)",             cmd_color},
    {"exit",     "Exit shell (same as halt)",         cmd_exit},
    {"halt",     "Halt the system",                   cmd_halt},
    {"reboot",   "Reboot the system",                 cmd_reboot},
    /* Filesystem */
    {"ls",       "List directory contents",           cmd_ls},
    {"cd",       "Change directory: cd <path>",       cmd_cd},
    {"pwd",      "Print working directory",           cmd_pwd},
    {"cat",      "Display file contents: cat <file>", cmd_cat},
    {"touch",    "Create empty file: touch <name>",   cmd_touch},
    {"mkdir",    "Create directory: mkdir <name>",    cmd_mkdir},
    {"rmdir",    "Remove empty directory: rmdir <name>", cmd_rmdir},
    {"rm",       "Remove file: rm <name>",            cmd_rm},
    {"write",    "Write to file: write <file> <text>",cmd_write},
    {"append",   "Append to file: append <file> <text>", cmd_append},
    {"cp",       "Copy file: cp <src> <dest>",        cmd_cp},
    {"mv",       "Move/rename: mv <src> <dest>",      cmd_mv},
    {"stat",     "Show file info: stat <name>",       cmd_stat},
    {"tree",     "Show directory tree: tree [path]",  cmd_tree},
    /* Core Utilities */
    {"grep",     "Pattern match: grep [-in] <pat> <file>", cmd_grep},
    {"find",     "File search: find [path] [-name pat]",   cmd_find},
    {"wc",       "Word/line count: wc [-lwc] <file>",      cmd_wc},
    {"head",     "First lines: head [-n N] <file>",        cmd_head},
    {"tail",     "Last lines: tail [-n N] <file>",         cmd_tail},
    {"sort",     "Sort lines: sort [-r] <file>",           cmd_sort},
    {"uniq",     "Remove dupes: uniq [-cd] <file>",        cmd_uniq},
    {"diff",     "Compare files: diff <f1> <f2>",          cmd_diff},
    {"tar",      "Archive: tar <list|create|extract> <arc>", cmd_tar},
    /* Memory */
    {"mem",       "Show memory information",           cmd_mem},
    {"free",      "Show memory usage (Linux-style)",   cmd_free},
    {"hexdump",   "Dump memory: hexdump <addr> [len]", cmd_hexdump},
    {"peek",      "Read memory: peek <addr>",          cmd_peek},
    {"poke",      "Write memory: poke <addr> <val>",   cmd_poke},
    {"alloc",     "Allocate memory: alloc <size>",     cmd_alloc},
    {"memtest",   "Test memory allocation/mapping",    cmd_memtest},
    {"heapstats", "Show heap allocation statistics",   cmd_heapstats},
    {"leaktest",  "Test memory leak detection",        cmd_leaktest},
    /* Disk */
    {"lsblk",    "List block devices (disks)",        cmd_lsblk},
    {"hdinfo",   "Show disk info: hdinfo <drive>",    cmd_hdinfo},
    {"readsec",  "Read sector: readsec <drv> <lba>",  cmd_readsec},
    {"fatmount", "Mount FAT32: fatmount <drive>",     cmd_fatmount},
    {"fatls",    "List FAT32 dir: fatls [path]",      cmd_fatls},
    {"fatcat",   "Read FAT32 file: fatcat <file>",    cmd_fatcat},
    {"mounts",   "Show mounted filesystems",          cmd_mounts},
    /* Process */
    {"ps",       "List running processes",            cmd_ps},
    {"kill",     "Kill a process: kill <pid>",        cmd_kill},
    {"tasks",    "List scheduler tasks",              cmd_tasks},
    {"exec",     "Execute user program",              cmd_exec},
    /* System */
    {"time",     "Show current time",                 cmd_time},
    {"date",     "Show current date",                 cmd_date},
    {"timezone", "Set timezone: timezone <offset>",   cmd_timezone},
    {"lspci",    "List PCI devices",                  cmd_lspci},
    {"apic",     "Show APIC status and info",         cmd_apic},
    {"drivers",  "List registered PCI drivers",       cmd_drivers},
    {"symbols",  "Show kernel symbol addresses",      cmd_symbols},
    {"isolation","Driver isolation status",            cmd_isolation},
    /* Debug */
    {"panic",      "Test kernel panic handler",         cmd_panic},
    {"vga",        "Write to VGA buffer directly",      cmd_vga},
    {"beep",       "Play a beep: beep [freq] [ms]",     cmd_beep},
    {"play",       "Play a tune",                       cmd_play},
    {"synctest",   "Test mutex/semaphore blocking",     cmd_synctest},
    {"pritest",    "Test priority inheritance",         cmd_pritest},
    {"cvtest",     "Test condition variables",          cmd_cvtest},
    {"rwtest",     "Test read-write locks",             cmd_rwtest},
    {"asserttest", "Test ASSERT macro (will halt)",     cmd_asserttest},
    {"guardtest",  "Test memory guard detection",       cmd_guardtest},
    /* Network */
    {"netinit",    "Initialize network stack",          cmd_netinit},
    {"ifconfig",   "Show/set IP config",                cmd_ifconfig},
    {"ping",       "Send ICMP ping: ping <ip>",         cmd_ping},
    {"arp",        "Show ARP cache",                    cmd_arp},
    {"netpoll",    "Poll network for packets",          cmd_netpoll},
    {"netstat",    "Show active connections",           cmd_netstat},
    {"dhcp",       "Get IP via DHCP",                   cmd_dhcp},
    {"dns",        "Resolve hostname: dns <host>",      cmd_dns},
    {"route",      "Show routing table",                cmd_route},
    /* Security */
    {"security",   "Show security status",              cmd_security},
    {"users",      "List user accounts",                cmd_users},
    {"groups",     "List groups",                       cmd_groups},
    {"useradd",    "Add user: useradd <name> <uid> <gid>", cmd_useradd},
    {"groupadd",   "Add group: groupadd <name> <gid>",  cmd_groupadd},
    {"aslr",       "ASLR control: aslr <on|off>",       cmd_aslr},
    {"whoami",     "Show current user",                 cmd_whoami},
    {"id",         "Show user/group IDs",               cmd_id},
    /* Shell builtins */
    {"export",     "Set/show env vars: export VAR=val",  cmd_export},
    {"unset",      "Remove env variable: unset VAR",     cmd_unset},
    {"env",        "Show all environment variables",     cmd_env},
    {"set",        "Set variable: set VAR value",        cmd_set},
    {"source",     "Run script: source <file>",          cmd_source},
    {"jobs",       "List background jobs",               cmd_jobs},
    {"fg",         "Bring job to foreground",            cmd_fg},
    {"history",    "Show command history",               cmd_history},
    {"alias",      "Define alias: alias name=value",     cmd_alias},
    {NULL, NULL, NULL}
};

static void print_category_commands(shell_command_t *cmds)
{
    for (int i = 0; cmds[i].name != NULL; i++) {
        vga_puts("  ");
        vga_puts(cmds[i].name);
        vga_puts(" - ");
        vga_puts(cmds[i].description);
        vga_puts("\n");
    }
}

void cmd_help(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Command Categories:\n");
        vga_puts("-------------------\n");
        for (int i = 0; categories[i].name != NULL; i++) {
            vga_puts("  ");
            vga_puts(categories[i].name);
            vga_puts(" - ");
            vga_puts(categories[i].description);
            vga_puts("\n");
        }
        vga_puts("\nType 'help <category>' to see commands in that category.\n");
        vga_puts("Type 'help all' to see all commands.\n");
        return;
    }
    
    if (strcmp(argv[1], "general") == 0) {
        vga_puts("General Commands:\n");
        print_category_commands(general_commands);
    } else if (strcmp(argv[1], "fs") == 0) {
        vga_puts("Filesystem Commands:\n");
        print_category_commands(fs_commands);
    } else if (strcmp(argv[1], "mem") == 0) {
        vga_puts("Memory Commands:\n");
        print_category_commands(mem_commands);
    } else if (strcmp(argv[1], "disk") == 0) {
        vga_puts("Disk Commands:\n");
        print_category_commands(disk_commands);
    } else if (strcmp(argv[1], "process") == 0) {
        vga_puts("Process Commands:\n");
        print_category_commands(process_commands);
    } else if (strcmp(argv[1], "system") == 0) {
        vga_puts("System Commands:\n");
        print_category_commands(system_commands);
    } else if (strcmp(argv[1], "debug") == 0) {
        vga_puts("Debug Commands:\n");
        print_category_commands(debug_commands);
    } else if (strcmp(argv[1], "net") == 0) {
        vga_puts("Network Commands:\n");
        print_category_commands(net_commands);
    } else if (strcmp(argv[1], "security") == 0) {
        vga_puts("Security Commands:\n");
        print_category_commands(security_commands);
    } else if (strcmp(argv[1], "shell") == 0) {
        vga_puts("Shell Builtins:\n");
        print_category_commands(shell_builtins);
    } else if (strcmp(argv[1], "utils") == 0) {
        cmd_utils(argc, argv);
    } else if (strcmp(argv[1], "all") == 0) {
        vga_puts("All Available Commands:\n");
        vga_puts("=======================\n\n");
        vga_puts("[General]\n");
        print_category_commands(general_commands);
        vga_puts("\n[Filesystem]\n");
        print_category_commands(fs_commands);
        vga_puts("\n[Memory]\n");
        print_category_commands(mem_commands);
        vga_puts("\n[Disk]\n");
        print_category_commands(disk_commands);
        vga_puts("\n[Process]\n");
        print_category_commands(process_commands);
        vga_puts("\n[System]\n");
        print_category_commands(system_commands);
        vga_puts("\n[Debug]\n");
        print_category_commands(debug_commands);
        vga_puts("\n[Network]\n");
        print_category_commands(net_commands);
        vga_puts("\n[Security]\n");
        print_category_commands(security_commands);
        vga_puts("\n[Shell]\n");
        print_category_commands(shell_builtins);
        vga_puts("\n[Utils]\n");
        print_category_commands(utils_commands);
    } else {
        vga_puts("Unknown category: ");
        vga_puts(argv[1]);
        vga_puts("\nAvailable categories: general, fs, mem, disk, process, system, debug, net, security, shell, utils\n");
    }
}

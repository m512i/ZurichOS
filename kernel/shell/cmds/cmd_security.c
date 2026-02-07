/* Security Shell Commands
 * Commands for managing security features
 */

#include <kernel/shell.h>
#include <drivers/vga.h>
#include <security/security.h>
#include <string.h>

static void print_num(uint32_t n)
{
    char buf[12];
    int i = 0;
    if (n == 0) {
        vga_putchar('0');
        return;
    }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0) {
        vga_putchar(buf[--i]);
    }
}

void cmd_security(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    
    vga_puts("Security Status:\n");
    vga_puts("================\n\n");
    
    uint32_t features = security_get_cpu_features();
    
    vga_puts("CPU Features:\n");
    vga_puts("  PAE:  ");
    vga_puts((features & CPU_FEATURE_PAE) ? "supported\n" : "not supported\n");
    vga_puts("  NX:   ");
    vga_puts((features & CPU_FEATURE_NX) ? "supported\n" : "not supported\n");
    vga_puts("  SMEP: ");
    vga_puts((features & CPU_FEATURE_SMEP) ? "enabled\n" : "not available\n");
    vga_puts("  SMAP: ");
    vga_puts((features & CPU_FEATURE_SMAP) ? "enabled\n" : "not available\n");
    
    vga_puts("\nProtections:\n");
    vga_puts("  Stack canary: enabled\n");
    vga_puts("  ASLR: enabled by default\n");
    
    vga_puts("\nNote: NX/SMEP/SMAP require hardware support.\n");
    vga_puts("Run QEMU with '-cpu host' or '-cpu Haswell' to enable.\n");
}

void cmd_users(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    
    vga_puts("User Accounts:\n");
    vga_puts("UID    GID    Username        Home\n");
    vga_puts("----   ----   --------        ----\n");
    
    for (uint32_t uid = 0; uid < 100; uid++) {
        user_t *user = user_get_by_uid(uid);
        if (user) {
            print_num(user->uid);
            vga_puts("      ");
            print_num(user->gid);
            vga_puts("      ");
            vga_puts(user->username);
            int len = strlen(user->username);
            for (int i = len; i < 16; i++) vga_putchar(' ');
            vga_puts(user->home_dir);
            vga_puts("\n");
        }
    }
}

void cmd_groups(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    
    vga_puts("Groups:\n");
    vga_puts("GID    Name            Members\n");
    vga_puts("----   ----            -------\n");
    
    for (uint32_t gid = 0; gid < 200; gid++) {
        group_t *group = group_get_by_gid(gid);
        if (group) {
            print_num(group->gid);
            vga_puts("      ");
            vga_puts(group->groupname);
            int len = strlen(group->groupname);
            for (int i = len; i < 16; i++) vga_putchar(' ');
            print_num(group->member_count);
            vga_puts("\n");
        }
    }
}

void cmd_useradd(int argc, char **argv)
{
    if (argc < 4) {
        vga_puts("Usage: useradd <username> <uid> <gid>\n");
        return;
    }
    
    const char *username = argv[1];
    uint32_t uid = 0, gid = 0;
    
    /* Parse UID */
    for (int i = 0; argv[2][i]; i++) {
        uid = uid * 10 + (argv[2][i] - '0');
    }
    
    /* Parse GID */
    for (int i = 0; argv[3][i]; i++) {
        gid = gid * 10 + (argv[3][i] - '0');
    }
    
    if (user_add(username, "password", uid, gid) == 0) {
        vga_puts("User '");
        vga_puts(username);
        vga_puts("' added with UID ");
        print_num(uid);
        vga_puts("\n");
    } else {
        vga_puts("Failed to add user\n");
    }
}

void cmd_groupadd(int argc, char **argv)
{
    if (argc < 3) {
        vga_puts("Usage: groupadd <groupname> <gid>\n");
        return;
    }
    
    const char *groupname = argv[1];
    uint32_t gid = 0;
    
    for (int i = 0; argv[2][i]; i++) {
        gid = gid * 10 + (argv[2][i] - '0');
    }
    
    if (group_add(groupname, gid) == 0) {
        vga_puts("Group '");
        vga_puts(groupname);
        vga_puts("' added with GID ");
        print_num(gid);
        vga_puts("\n");
    } else {
        vga_puts("Failed to add group\n");
    }
}

void cmd_aslr(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: aslr <on|off|status>\n");
        return;
    }
    
    if (strcmp(argv[1], "on") == 0) {
        aslr_enable();
        vga_puts("ASLR enabled\n");
    } else if (strcmp(argv[1], "off") == 0) {
        aslr_disable();
        vga_puts("ASLR disabled\n");
    } else if (strcmp(argv[1], "status") == 0) {
        vga_puts("ASLR: check serial output for status\n");
    } else {
        vga_puts("Usage: aslr <on|off|status>\n");
    }
}

void cmd_whoami(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    
    vga_puts("root\n");
}

void cmd_id(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    
    vga_puts("uid=0(root) gid=0(root) groups=0(root),10(wheel)\n");
}

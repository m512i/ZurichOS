/* Shell Commands - Shell Builtins
 * export, unset, env, set, source, jobs, fg, history, alias
 */

#include <shell/builtins.h>
#include <shell/shell_features.h>
#include <kernel/shell.h>
#include <drivers/vga.h>
#include <drivers/serial.h>
#include <string.h>

void cmd_export(int argc, char **argv)
{
    if (argc < 2) {
        env_list();
        return;
    }
    
    for (int i = 1; i < argc; i++) {
        char *eq = NULL;
        for (char *p = argv[i]; *p; p++) {
            if (*p == '=') { eq = p; break; }
        }
        
        if (eq) {
            *eq = '\0';
            env_set(argv[i], eq + 1);
            *eq = '=';
        } else {
            const char *val = env_get(argv[i]);
            if (val) {
                vga_puts(argv[i]);
                vga_puts("=");
                vga_puts(val);
                vga_puts("\n");
            }
        }
    }
}

void cmd_unset(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: unset <variable>\n");
        return;
    }
    
    for (int i = 1; i < argc; i++) {
        env_unset(argv[i]);
    }
}

void cmd_env(int argc, char **argv)
{
    (void)argc; (void)argv;
    env_list();
}

void cmd_set(int argc, char **argv)
{
    if (argc < 3) {
        env_list();
        return;
    }
    
    env_set(argv[1], argv[2]);
}

void cmd_source(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: source <script>\n");
        return;
    }
    
    shell_run_script(argv[1]);
}

void cmd_jobs(int argc, char **argv)
{
    (void)argc; (void)argv;
    jobs_list();
}

void cmd_fg(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_puts("fg: no job control in single-threaded kernel\n");
}

extern char history[][256];
extern int history_count;
extern int history_write;

void cmd_history(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    if (history_count == 0) {
        vga_puts("No history.\n");
        return;
    }
    
    int start = 0;
    if (history_count >= 16) {
        start = history_write;
    }
    
    for (int i = 0; i < history_count; i++) {
        int idx = (start + i) % 16;
        
        char num[8];
        int n = i + 1;
        int ni = 0;
        if (n == 0) { num[ni++] = '0'; }
        else {
            char tmp[8]; int ti = 0;
            while (n > 0) { tmp[ti++] = '0' + (n % 10); n /= 10; }
            while (ti > 0) num[ni++] = tmp[--ti];
        }
        num[ni] = '\0';
        
        for (int j = ni; j < 4; j++) vga_puts(" ");
        vga_puts(num);
        vga_puts("  ");
        vga_puts(history[idx]);
        vga_puts("\n");
    }
}

#define MAX_ALIASES 16
#define MAX_ALIAS_NAME 32
#define MAX_ALIAS_VALUE 128

static struct {
    char name[MAX_ALIAS_NAME];
    char value[MAX_ALIAS_VALUE];
    int in_use;
} aliases[MAX_ALIASES];

void cmd_alias(int argc, char **argv)
{
    if (argc < 2) {
        int found = 0;
        for (int i = 0; i < MAX_ALIASES; i++) {
            if (aliases[i].in_use) {
                found = 1;
                vga_puts("alias ");
                vga_puts(aliases[i].name);
                vga_puts("='");
                vga_puts(aliases[i].value);
                vga_puts("'\n");
            }
        }
        if (!found) {
            vga_puts("No aliases defined.\n");
        }
        return;
    }
    
    char *eq = NULL;
    for (char *p = argv[1]; *p; p++) {
        if (*p == '=') { eq = p; break; }
    }
    
    if (!eq) {
        for (int i = 0; i < MAX_ALIASES; i++) {
            if (aliases[i].in_use && strcmp(aliases[i].name, argv[1]) == 0) {
                vga_puts("alias ");
                vga_puts(aliases[i].name);
                vga_puts("='");
                vga_puts(aliases[i].value);
                vga_puts("'\n");
                return;
            }
        }
        vga_puts("alias: ");
        vga_puts(argv[1]);
        vga_puts(": not found\n");
        return;
    }
    
    *eq = '\0';
    char *name = argv[1];
    char *value = eq + 1;
    
    int vlen = strlen(value);
    if (vlen >= 2 && ((value[0] == '\'' && value[vlen-1] == '\'') ||
                       (value[0] == '"' && value[vlen-1] == '"'))) {
        value[vlen-1] = '\0';
        value++;
    }
    
    for (int i = 0; i < MAX_ALIASES; i++) {
        if (aliases[i].in_use && strcmp(aliases[i].name, name) == 0) {
            strncpy(aliases[i].value, value, MAX_ALIAS_VALUE - 1);
            aliases[i].value[MAX_ALIAS_VALUE - 1] = '\0';
            return;
        }
    }
    
    for (int i = 0; i < MAX_ALIASES; i++) {
        if (!aliases[i].in_use) {
            strncpy(aliases[i].name, name, MAX_ALIAS_NAME - 1);
            aliases[i].name[MAX_ALIAS_NAME - 1] = '\0';
            strncpy(aliases[i].value, value, MAX_ALIAS_VALUE - 1);
            aliases[i].value[MAX_ALIAS_VALUE - 1] = '\0';
            aliases[i].in_use = 1;
            return;
        }
    }
    
    vga_puts("alias: too many aliases\n");
}

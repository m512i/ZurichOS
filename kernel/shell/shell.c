/* ZurichOS Shell
 * Main shell logic, input handling, and history
 */

#include <shell/builtins.h>
#include <shell/shell_features.h>
#include <kernel/shell.h>
#include <kernel/kernel.h>
#include <drivers/vga.h>
#include <drivers/framebuffer.h>
#include <drivers/keyboard.h>
#include <drivers/serial.h>
#include <fs/vfs.h>
#include <string.h>

static char input_buffer[SHELL_BUFFER_SIZE];
static uint32_t input_pos = 0;

#define HISTORY_SIZE 16

char history[HISTORY_SIZE][SHELL_BUFFER_SIZE];
int history_count = 0;
static int history_index = 0; 
int history_write = 0;  

uint32_t shell_parse_hex(const char *str)
{
    uint32_t val = 0;
    
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
    }
    
    while (*str) {
        char c = *str++;
        uint8_t digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else break;
        val = (val << 4) | digit;
    }
    return val;
}

uint32_t shell_parse_dec(const char *str)
{
    uint32_t val = 0;
    while (*str >= '0' && *str <= '9') {
        val = val * 10 + (*str - '0');
        str++;
    }
    return val;
}

static int parse_command(char *input, char **argv, int max_args)
{
    int argc = 0;
    char *p = input;
    
    while (*p && argc < max_args) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        
        if (*p == '"') {
            p++;
            argv[argc++] = p;
            while (*p && *p != '"') p++;
            if (*p == '"') *p++ = '\0';
        } else if (*p == '\'') {
            p++;
            argv[argc++] = p;
            while (*p && *p != '\'') p++;
            if (*p == '\'') *p++ = '\0';
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }
    }
    
    return argc;
}

extern vfs_node_t *shell_get_cwd(void);
extern char *shell_get_cwd_path(void);

static void run_simple_command(char *input)
{
    char *argv[SHELL_MAX_ARGS];
    int argc = parse_command(input, argv, SHELL_MAX_ARGS);
    
    if (argc == 0) return;
    
    if (argc == 1 && argv[0]) {
        char *eq = NULL;
        for (char *p = argv[0]; *p; p++) {
            if (*p == '=') { eq = p; break; }
        }
        if (eq && eq != argv[0]) {
            *eq = '\0';
            env_set(argv[0], eq + 1);
            return;
        }
    }
    
    for (int i = 0; shell_commands[i].name != NULL; i++) {
        if (strcmp(argv[0], shell_commands[i].name) == 0) {
            shell_commands[i].handler(argc, argv);
            return;
        }
    }
    
    vfs_node_t *node = vfs_lookup(argv[0]);
    if (node && !vfs_is_directory(node)) {
        shell_run_script(argv[0]);
        return;
    }
    
    vga_puts("Unknown command: ");
    vga_puts(argv[0]);
    vga_puts("\nType 'help' for available commands.\n");
}

static int parse_redirections(char *input, redirect_t *redir_out, redirect_t *redir_in)
{
    redir_out->type = REDIR_NONE;
    redir_in->type = REDIR_NONE;
    
    char *p = input;
    while (*p) {
        if (*p == '>' && *(p + 1) == '>') {
            *p = '\0';
            p += 2;
            while (*p == ' ') p++;
            redir_out->type = REDIR_APPEND;
            int fi = 0;
            while (*p && *p != ' ' && fi < 127) {
                redir_out->filename[fi++] = *p++;
            }
            redir_out->filename[fi] = '\0';
        } else if (*p == '>') {
            *p = '\0';
            p++;
            while (*p == ' ') p++;
            redir_out->type = REDIR_OUT;
            int fi = 0;
            while (*p && *p != ' ' && fi < 127) {
                redir_out->filename[fi++] = *p++;
            }
            redir_out->filename[fi] = '\0';
        } else if (*p == '<') {
            *p = '\0';
            p++;
            while (*p == ' ') p++;
            redir_in->type = REDIR_IN;
            int fi = 0;
            while (*p && *p != ' ' && fi < 127) {
                redir_in->filename[fi++] = *p++;
            }
            redir_in->filename[fi] = '\0';
        } else {
            p++;
        }
    }
    return 0;
}

static void execute_with_output_redirect(char *cmd, redirect_t *redir)
{
    extern void vga_set_capture(uint8_t *buf, uint32_t max);
    extern uint32_t vga_get_capture_len(void);
    extern void vga_stop_capture(void);
    
    static uint8_t capture_buf[4096];
    vga_set_capture(capture_buf, sizeof(capture_buf));
    
    run_simple_command(cmd);
    
    uint32_t len = vga_get_capture_len();
    vga_stop_capture();
    
    if (redir->filename[0] == '\0') return;
    
    char fullpath[256];
    if (redir->filename[0] == '/') {
        strncpy(fullpath, redir->filename, 255);
    } else {
        char *cwdp = shell_get_cwd_path();
        if (cwdp && cwdp[0] != '\0' && !(cwdp[0] == '/' && cwdp[1] == '\0')) {
            strncpy(fullpath, cwdp, 200);
            strcat(fullpath, "/");
            strcat(fullpath, redir->filename);
        } else {
            fullpath[0] = '/';
            strncpy(fullpath + 1, redir->filename, 254);
        }
    }
    fullpath[255] = '\0';
    
    vfs_node_t *file = vfs_lookup(fullpath);
    
    if (!file) {
        char path_copy[256];
        strncpy(path_copy, fullpath, 255);
        path_copy[255] = '\0';
        
        char *last_slash = NULL;
        for (char *s = path_copy; *s; s++) {
            if (*s == '/') last_slash = s;
        }
        
        if (last_slash) {
            *last_slash = '\0';
            char *fname = last_slash + 1;
            vfs_node_t *parent;
            if (last_slash == path_copy) {
                parent = vfs_get_root();
            } else {
                parent = vfs_lookup(path_copy);
            }
            if (parent && vfs_is_directory(parent)) {
                vfs_create(parent, fname, VFS_FILE);
                file = vfs_finddir(parent, fname);
            }
        }
    }
    
    if (!file) {
        vga_puts("sh: cannot create: ");
        vga_puts(redir->filename);
        vga_puts("\n");
        return;
    }
    
    if (redir->type == REDIR_APPEND) {
        vfs_append(file, len, capture_buf);
    } else {
        vfs_truncate(file);
        vfs_write(file, 0, len, capture_buf);
    }
}

static void execute_with_input_redirect(char *cmd, redirect_t *redir_in)
{
    if (redir_in->filename[0] == '\0') {
        run_simple_command(cmd);
        return;
    }
    
    vfs_node_t *file = vfs_lookup(redir_in->filename);
    if (!file) {
        vga_puts("sh: ");
        vga_puts(redir_in->filename);
        vga_puts(": No such file\n");
        return;
    }
    
    run_simple_command(cmd);
}

static void execute_pipeline(char *input)
{
    char *cmds[8];
    int cmd_count = 0;
    char *p = input;
    cmds[cmd_count++] = p;
    
    while (*p && cmd_count < 8) {
        if (*p == '|') {
            *p = '\0';
            p++;
            while (*p == ' ') p++;
            cmds[cmd_count++] = p;
        } else {
            p++;
        }
    }
    
    if (cmd_count == 1) {
        redirect_t redir_out, redir_in;
        parse_redirections(cmds[0], &redir_out, &redir_in);
        
        if (redir_out.type != REDIR_NONE) {
            execute_with_output_redirect(cmds[0], &redir_out);
        } else if (redir_in.type != REDIR_NONE) {
            execute_with_input_redirect(cmds[0], &redir_in);
        } else {
            run_simple_command(cmds[0]);
        }
        return;
    }
    
    extern void vga_set_capture(uint8_t *buf, uint32_t max);
    extern uint32_t vga_get_capture_len(void);
    extern void vga_stop_capture(void);
    
    static uint8_t pipe_buf[4096];
    
    for (int i = 0; i < cmd_count; i++) {
        char *cmd = cmds[i];
        while (*cmd == ' ') cmd++;
        int len = strlen(cmd);
        while (len > 0 && cmd[len-1] == ' ') cmd[--len] = '\0';
        
        if (i < cmd_count - 1) {
            vga_set_capture(pipe_buf, sizeof(pipe_buf));
            run_simple_command(cmd);
            uint32_t cap_len = vga_get_capture_len();
            vga_stop_capture();
            (void)cap_len;
            /* Pipe data available for next command via pipe_buf
             * this would feed stdin of next command */
        } else {
            redirect_t redir_out, redir_in;
            parse_redirections(cmd, &redir_out, &redir_in);
            
            if (redir_out.type != REDIR_NONE) {
                execute_with_output_redirect(cmd, &redir_out);
            } else {
                run_simple_command(cmd);
            }
        }
    }
}

static void execute_command(char *input)
{
    char expanded[SHELL_BUFFER_SIZE];
    env_expand(input, expanded, SHELL_BUFFER_SIZE);
    
    int background = 0;
    int len = strlen(expanded);
    if (len > 0 && expanded[len - 1] == '&') {
        expanded[len - 1] = '\0';
        len--;
        while (len > 0 && expanded[len - 1] == ' ') expanded[--len] = '\0';
        background = 1;
    }
    
    if (background) {
        int jid = job_add(expanded);
        if (jid > 0) {
            vga_puts("[");
            char buf[8];
            int idx = 0;
            int n = jid;
            if (n == 0) { buf[idx++] = '0'; }
            else {
                char tmp[8]; int ti = 0;
                while (n > 0) { tmp[ti++] = '0' + (n % 10); n /= 10; }
                while (ti > 0) buf[idx++] = tmp[--ti];
            }
            buf[idx] = '\0';
            vga_puts(buf);
            vga_puts("] Running: ");
            vga_puts(expanded);
            vga_puts("\n");
            
            /* Execute immediately (no true background in single-threaded kernel) */
            execute_pipeline(expanded);
            job_set_state(jid, JOB_DONE);
        } else {
            vga_puts("sh: too many background jobs\n");
        }
    } else {
        jobs_check();
        execute_pipeline(expanded);
    }
    
    vfs_node_t *cwd = shell_get_cwd();
    if (cwd) {
        char path[VFS_MAX_PATH];
        char temp[VFS_MAX_PATH];
        path[0] = '\0';
        vfs_node_t *dir = cwd;
        int depth = 0;
        while (dir && depth < 32) {
            if (dir->parent && dir->name[0] != '\0') {
                strcpy(temp, "/");
                strcat(temp, dir->name);
                strcat(temp, path);
                strcpy(path, temp);
            }
            dir = dir->parent;
            depth++;
        }
        if (path[0] == '\0') strcpy(path, "/");
        env_set("PWD", path);
    }
}

void shell_execute_line(char *line)
{
    char copy[SHELL_BUFFER_SIZE];
    strncpy(copy, line, SHELL_BUFFER_SIZE - 1);
    copy[SHELL_BUFFER_SIZE - 1] = '\0';
    execute_command(copy);
}

static void print_prompt(void)
{
    vfs_node_t *cwd = shell_get_cwd();
    
    vga_puts("zurich:");
    
    if (cwd && cwd->name[0] != '\0') {
        char path[VFS_MAX_PATH];
        char temp[VFS_MAX_PATH];
        path[0] = '\0';
        
        vfs_node_t *dir = cwd;
        int depth = 0;
        while (dir && depth < 32) {
            if (dir->parent && dir->name[0] != '\0') {
                strcpy(temp, "/");
                strcat(temp, dir->name);
                strcat(temp, path);
                strcpy(path, temp);
            }
            dir = dir->parent;
            depth++;
        }
        
        if (path[0] == '\0') {
            vga_puts("/");
        } else {
            vga_puts(path);
        }
    } else {
        vga_puts("/");
    }
    
    vga_puts("> ");
}

static void history_add(const char *cmd)
{
    if (cmd[0] == '\0') return;
    
    if (history_count > 0) {
        int last = (history_write - 1 + HISTORY_SIZE) % HISTORY_SIZE;
        if (strcmp(history[last], cmd) == 0) return;
    }
    
    strcpy(history[history_write], cmd);
    history_write = (history_write + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE) {
        history_count++;
    }
}

static void clear_input_line(void)
{
    while (input_pos > 0) {
        vga_putchar('\b');
        input_pos--;
    }
}

static void set_input(const char *str)
{
    clear_input_line();
    strcpy(input_buffer, str);
    input_pos = strlen(str);
    vga_puts(input_buffer);
}

static void do_tab_completion(void)
{
    if (input_pos == 0) return;
    
    input_buffer[input_pos] = '\0';
    
    char *word_start = input_buffer;
    for (int i = input_pos - 1; i >= 0; i--) {
        if (input_buffer[i] == ' ') {
            word_start = &input_buffer[i + 1];
            break;
        }
    }
    
    int word_len = strlen(word_start);
    if (word_len == 0) return;
    
    int is_command = (word_start == input_buffer);
    
    char *match = NULL;
    int match_count = 0;
    
    if (is_command) {
        for (int i = 0; shell_commands[i].name != NULL; i++) {
            if (strncmp(shell_commands[i].name, word_start, word_len) == 0) {
                if (match_count == 0) {
                    match = (char *)shell_commands[i].name;
                }
                match_count++;
            }
        }
    } else {
        vfs_node_t *cwd = shell_get_cwd();
        if (cwd) {
            uint32_t index = 0;
            dirent_t *entry;
            while ((entry = vfs_readdir(cwd, index++)) != NULL) {
                if (strncmp(entry->name, word_start, word_len) == 0) {
                    if (match_count == 0) {
                        match = entry->name;
                    }
                    match_count++;
                }
            }
        }
    }
    
    if (match_count == 1 && match) {
        strcpy(word_start, match);
        strcat(input_buffer, " ");
        clear_input_line();
        input_pos = strlen(input_buffer);
        vga_puts(input_buffer);
    } else if (match_count > 1) {
        vga_puts("\n");
        
        if (is_command) {
            for (int i = 0; shell_commands[i].name != NULL; i++) {
                if (strncmp(shell_commands[i].name, word_start, word_len) == 0) {
                    vga_puts(shell_commands[i].name);
                    vga_puts("  ");
                }
            }
        } else {
            vfs_node_t *cwd = shell_get_cwd();
            if (cwd) {
                uint32_t index = 0;
                dirent_t *entry;
                while ((entry = vfs_readdir(cwd, index++)) != NULL) {
                    if (strncmp(entry->name, word_start, word_len) == 0) {
                        vga_puts(entry->name);
                        vga_puts("  ");
                    }
                }
            }
        }
        
        vga_puts("\n");
        print_prompt();
        vga_puts(input_buffer);
    }
}

static int escape_state = 0;

void shell_input(char c)
{
    if (escape_state == 1) {
        if (c == '[') {
            escape_state = 2;
            return;
        }
        escape_state = 0;
    } else if (escape_state == 2) {
        escape_state = 0;
        if (c == 'A') {
            if (history_count > 0) {
                if (history_index > 0) {
                    history_index--;
                }
                int idx = (history_write - history_count + history_index + HISTORY_SIZE) % HISTORY_SIZE;
                set_input(history[idx]);
            }
            return;
        } else if (c == 'B') {
            if (history_index < history_count - 1) {
                history_index++;
                int idx = (history_write - history_count + history_index + HISTORY_SIZE) % HISTORY_SIZE;
                set_input(history[idx]);
            } else if (history_index == history_count - 1) {
                history_index = history_count;
                clear_input_line();
                input_buffer[0] = '\0';
            }
            return;
        }
        return;
    }
    
    if (c == '\x1B') {
        escape_state = 1;
        return;
    }
    
    if (c == '\n') {
        vga_putchar('\n');
        input_buffer[input_pos] = '\0';
        
        if (input_pos > 0) {
            history_add(input_buffer);
            execute_command(input_buffer);
        }
        
        history_index = history_count;
        
        input_pos = 0;
        print_prompt();
    } else if (c == '\b') {
        if (input_pos > 0) {
            input_pos--;
            vga_putchar('\b');
        }
    } else if (c == '\t') {
        do_tab_completion();
    } else if (c == 3) {
        vga_puts("^C\n");
        input_pos = 0;
        input_buffer[0] = '\0';
        history_index = history_count;
        print_prompt();
    } else if (c >= 32 && c < 127) {
        if (input_pos < SHELL_BUFFER_SIZE - 1) {
            input_buffer[input_pos++] = c;
            vga_putchar(c);
        }
    }
}

void shell_init(void)
{
    input_pos = 0;
    input_buffer[0] = '\0';
    
    serial_puts("[SHELL] shell_init start\n");
    
    env_init();
    jobs_init();
    
    /* Source /etc/profile if it exists (loads persistent env from disk) */
    vfs_node_t *profile = vfs_lookup("/etc/profile");
    if (profile && profile->length > 0) {
        serial_puts("[SHELL] Sourcing /etc/profile\n");
        shell_run_script("/etc/profile");
    }
    
    vga_puts("\n");
    vga_puts("ZurichOS Shell v0.2\n");
    vga_puts("Type 'help' for available commands.\n\n");
    
    serial_puts("[SHELL] Setting keyboard callback\n");
    keyboard_set_callback(shell_input);
    
    serial_puts("[SHELL] Calling print_prompt\n");
    print_prompt();
    serial_puts("[SHELL] shell_init done\n");
}

void shell_run(void)
{
    serial_puts("[SHELL] Entering shell_run\n");
    while (1) {
        keyboard_process_events();
        fb_flush();
        __asm__ volatile("sti; hlt");
    }
}

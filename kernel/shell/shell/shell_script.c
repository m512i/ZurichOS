/* Shell Scripting
 * Basic script execution from files
 */

#include <shell/shell_features.h>
#include <shell/builtins.h>
#include <kernel/shell.h>
#include <drivers/vga.h>
#include <drivers/serial.h>
#include <fs/vfs.h>
#include <string.h>

extern void shell_execute_line(char *line);

int shell_run_script(const char *path)
{
    vfs_node_t *node = vfs_lookup(path);
    if (!node) {
        vga_puts("sh: ");
        vga_puts(path);
        vga_puts(": No such file\n");
        return -1;
    }
    
    if (node->length == 0) {
        return 0;
    }
    
    if (node->length > 4096) {
        vga_puts("sh: script too large\n");
        return -1;
    }
    
    uint8_t buf[4096];
    int bytes = vfs_read(node, 0, node->length, buf);
    if (bytes <= 0) {
        vga_puts("sh: cannot read script\n");
        return -1;
    }
    buf[bytes] = '\0';
    
    char *data = (char *)buf;
    
    if (data[0] == '#' && data[1] == '!') {
        while (*data && *data != '\n') data++;
        if (*data == '\n') data++;
    }
    
    char line[SHELL_BUFFER_SIZE];
    int line_num = 0;
    
    while (*data) {
        int li = 0;
        line_num++;
        
        while (*data && *data != '\n' && li < SHELL_BUFFER_SIZE - 1) {
            line[li++] = *data++;
        }
        line[li] = '\0';
        if (*data == '\n') data++;
        
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '\0' || *trimmed == '#') continue;
        
        serial_puts("[SCRIPT] Executing: ");
        serial_puts(trimmed);
        serial_puts("\n");
        
        shell_execute_line(trimmed);
    }
    
    return 0;
}

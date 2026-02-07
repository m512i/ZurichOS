/* Shell Commands - Filesystem Category
 * ls, cd, pwd, cat, touch, mkdir, rmdir, rm, write, append, cp, mv, stat
 */

#include <shell/builtins.h>
#include <kernel/shell.h>
#include <kernel/kernel.h>
#include <drivers/vga.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <string.h>

static vfs_node_t *current_dir = NULL;

static vfs_node_t *get_cwd(void)
{
    if (!current_dir) {
        current_dir = vfs_get_root();
    }
    return current_dir;
}

vfs_node_t *shell_get_cwd(void)
{
    return get_cwd();
}

static char cwd_path_buf[256];

char *shell_get_cwd_path(void)
{
    vfs_node_t *cwd = get_cwd();
    if (!cwd) {
        cwd_path_buf[0] = '/';
        cwd_path_buf[1] = '\0';
        return cwd_path_buf;
    }
    
    char temp[256];
    cwd_path_buf[0] = '\0';
    
    vfs_node_t *dir = cwd;
    int depth = 0;
    while (dir && depth < 32) {
        if (dir->parent && dir->name[0] != '\0') {
            strcpy(temp, "/");
            strcat(temp, dir->name);
            strcat(temp, cwd_path_buf);
            strcpy(cwd_path_buf, temp);
        }
        dir = dir->parent;
        depth++;
    }
    
    if (cwd_path_buf[0] == '\0') {
        cwd_path_buf[0] = '/';
        cwd_path_buf[1] = '\0';
    }
    
    return cwd_path_buf;
}

void cmd_ls(int argc, char **argv)
{
    vfs_node_t *dir = get_cwd();
    
    if (argc >= 2) {
        if (argv[1][0] == '/') {
            dir = vfs_lookup(argv[1]);
        } else {
            dir = vfs_finddir(get_cwd(), argv[1]);
        }
    }
    
    if (!dir) {
        vga_puts("ls: directory not found\n");
        return;
    }
    
    if (!vfs_is_directory(dir)) {
        vga_puts(dir->name);
        vga_puts("  ");
        vga_put_dec(dir->length);
        vga_puts(" bytes\n");
        return;
    }
    
    uint32_t index = 0;
    dirent_t *entry;
    
    while ((entry = vfs_readdir(dir, index++)) != NULL) {
        vfs_node_t *child = vfs_finddir(dir, entry->name);
        
        if (child && vfs_is_directory(child)) {
            vga_puts("[DIR]  ");
        } else {
            vga_puts("       ");
        }
        
        vga_puts(entry->name);
        
        if (child && !vfs_is_directory(child)) {
            vga_puts("  (");
            vga_put_dec(child->length);
            vga_puts(" bytes)");
        }
        vga_puts("\n");
    }
    
    if (index == 1) {
        vga_puts("(empty directory)\n");
    }
}

void cmd_cd(int argc, char **argv)
{
    if (argc < 2) {
        current_dir = vfs_get_root();
        return;
    }
    
    vfs_node_t *target;
    
    if (strcmp(argv[1], "..") == 0) {
        if (get_cwd()->parent) {
            current_dir = get_cwd()->parent;
        }
        return;
    }
    
    if (argv[1][0] == '/') {
        target = vfs_lookup(argv[1]);
    } else {
        target = vfs_finddir(get_cwd(), argv[1]);
    }
    
    if (!target) {
        vga_puts("cd: directory not found: ");
        vga_puts(argv[1]);
        vga_puts("\n");
        return;
    }
    
    if (!vfs_is_directory(target)) {
        vga_puts("cd: not a directory: ");
        vga_puts(argv[1]);
        vga_puts("\n");
        return;
    }
    
    current_dir = target;
}

void cmd_pwd(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vfs_node_t *dir = get_cwd();
    
    char path[VFS_MAX_PATH];
    char temp[VFS_MAX_PATH];
    path[0] = '\0';
    
    while (dir) {
        if (dir->parent) {
            strcpy(temp, "/");
            strcat(temp, dir->name);
            strcat(temp, path);
            strcpy(path, temp);
        }
        dir = dir->parent;
    }
    
    if (path[0] == '\0') {
        vga_puts("/\n");
    } else {
        vga_puts(path);
        vga_puts("\n");
    }
}

void cmd_cat(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: cat <filename>\n");
        return;
    }
    
    vfs_node_t *file;
    if (argv[1][0] == '/') {
        file = vfs_lookup(argv[1]);
    } else {
        file = vfs_finddir(get_cwd(), argv[1]);
    }
    
    if (!file) {
        vga_puts("cat: file not found: ");
        vga_puts(argv[1]);
        vga_puts("\n");
        return;
    }
    
    if (vfs_is_directory(file)) {
        vga_puts("cat: is a directory: ");
        vga_puts(argv[1]);
        vga_puts("\n");
        return;
    }
    
    if (file->length == 0) {
        return;  
    }
    
    uint8_t *buffer = kmalloc(file->length + 1);
    if (!buffer) {
        vga_puts("cat: out of memory\n");
        return;
    }
    
    int bytes_read = vfs_read(file, 0, file->length, buffer);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        vga_puts((char *)buffer);
        if (buffer[bytes_read - 1] != '\n') {
            vga_puts("\n");
        }
    }
    
    kfree(buffer);
}

void cmd_touch(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: touch <filename>\n");
        return;
    }
    
    vfs_node_t *existing = vfs_finddir(get_cwd(), argv[1]);
    if (existing) {
        return;
    }
    
    if (vfs_create(get_cwd(), argv[1], VFS_FILE) != 0) {
        vga_puts("touch: failed to create file\n");
    }
}

void cmd_mkdir(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: mkdir <dirname>\n");
        return;
    }
    
    vfs_node_t *existing = vfs_finddir(get_cwd(), argv[1]);
    if (existing) {
        vga_puts("mkdir: already exists: ");
        vga_puts(argv[1]);
        vga_puts("\n");
        return;
    }
    
    if (vfs_create(get_cwd(), argv[1], VFS_DIRECTORY) != 0) {
        vga_puts("mkdir: failed to create directory\n");
    }
}

void cmd_rmdir(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: rmdir <dirname>\n");
        return;
    }
    
    vfs_node_t *target = vfs_finddir(get_cwd(), argv[1]);
    if (!target) {
        vga_puts("rmdir: not found: ");
        vga_puts(argv[1]);
        vga_puts("\n");
        return;
    }
    
    if (!vfs_is_directory(target)) {
        vga_puts("rmdir: not a directory: ");
        vga_puts(argv[1]);
        vga_puts("\n");
        return;
    }
    
    if (vfs_unlink(get_cwd(), argv[1]) != 0) {
        vga_puts("rmdir: directory not empty or failed: ");
        vga_puts(argv[1]);
        vga_puts("\n");
    }
}

void cmd_rm(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: rm <filename>\n");
        return;
    }
    
    vfs_node_t *target = vfs_finddir(get_cwd(), argv[1]);
    if (!target) {
        vga_puts("rm: not found: ");
        vga_puts(argv[1]);
        vga_puts("\n");
        return;
    }
    
    if (vfs_is_directory(target)) {
        vga_puts("rm: is a directory (use rmdir): ");
        vga_puts(argv[1]);
        vga_puts("\n");
        return;
    }
    
    if (vfs_unlink(get_cwd(), argv[1]) != 0) {
        vga_puts("rm: failed to remove: ");
        vga_puts(argv[1]);
        vga_puts("\n");
    }
}

void cmd_write(int argc, char **argv)
{
    if (argc < 3) {
        vga_puts("Usage: write <filename> <text...>\n");
        return;
    }
    
    vfs_node_t *file = vfs_finddir(get_cwd(), argv[1]);
    if (!file) {
        if (vfs_create(get_cwd(), argv[1], VFS_FILE) != 0) {
            vga_puts("write: failed to create file\n");
            return;
        }
        file = vfs_finddir(get_cwd(), argv[1]);
    }
    
    if (!file) {
        vga_puts("write: file not found\n");
        return;
    }
    
    if (vfs_is_directory(file)) {
        vga_puts("write: is a directory\n");
        return;
    }
    
    char content[1024];
    content[0] = '\0';
    
    for (int i = 2; i < argc; i++) {
        if (i > 2) strcat(content, " ");
        strcat(content, argv[i]);
    }
    strcat(content, "\n");
    
    int len = strlen(content);
    if (vfs_write(file, 0, len, (uint8_t *)content) != len) {
        vga_puts("write: failed to write\n");
    }
}

void cmd_append(int argc, char **argv)
{
    if (argc < 3) {
        vga_puts("Usage: append <filename> <text...>\n");
        return;
    }
    
    vfs_node_t *file = vfs_finddir(get_cwd(), argv[1]);
    if (!file) {
        if (vfs_create(get_cwd(), argv[1], VFS_FILE) != 0) {
            vga_puts("append: failed to create file\n");
            return;
        }
        file = vfs_finddir(get_cwd(), argv[1]);
    }
    
    if (!file) {
        vga_puts("append: file not found\n");
        return;
    }
    
    if (vfs_is_directory(file)) {
        vga_puts("append: is a directory\n");
        return;
    }
    
    char content[1024];
    content[0] = '\0';
    
    for (int i = 2; i < argc; i++) {
        if (i > 2) strcat(content, " ");
        strcat(content, argv[i]);
    }
    strcat(content, "\n");
    
    int len = strlen(content);
    if (vfs_append(file, len, (uint8_t *)content) != len) {
        vga_puts("append: failed to append\n");
    }
}

void cmd_cp(int argc, char **argv)
{
    if (argc < 3) {
        vga_puts("Usage: cp <source> <dest>\n");
        vga_puts("  dest can be a directory or new filename\n");
        return;
    }
    
    vfs_node_t *src;
    if (argv[1][0] == '/') {
        src = vfs_lookup(argv[1]);
    } else {
        src = vfs_finddir(get_cwd(), argv[1]);
    }
    
    if (!src) {
        vga_puts("cp: source not found: ");
        vga_puts(argv[1]);
        vga_puts("\n");
        return;
    }
    
    if (vfs_is_directory(src)) {
        vga_puts("cp: cannot copy directories\n");
        return;
    }
    
    vfs_node_t *dest_dir = NULL;
    char dest_name[VFS_MAX_NAME];
    
    if (argv[2][0] == '/') {
        vfs_node_t *dest_node = vfs_lookup(argv[2]);
        if (dest_node && vfs_is_directory(dest_node)) {
            dest_dir = dest_node;
            strncpy(dest_name, src->name, VFS_MAX_NAME - 1);
        } else if (dest_node) {
            vga_puts("cp: destination exists: ");
            vga_puts(argv[2]);
            vga_puts("\n");
            return;
        } else {
            char path_copy[VFS_MAX_PATH];
            strncpy(path_copy, argv[2], VFS_MAX_PATH - 1);
            
            char *last_slash = NULL;
            for (char *p = path_copy; *p; p++) {
                if (*p == '/') last_slash = p;
            }
            
            if (last_slash && last_slash != path_copy) {
                *last_slash = '\0';
                dest_dir = vfs_lookup(path_copy);
                strncpy(dest_name, last_slash + 1, VFS_MAX_NAME - 1);
            } else if (last_slash == path_copy) {
                dest_dir = vfs_get_root();
                strncpy(dest_name, last_slash + 1, VFS_MAX_NAME - 1);
            }
        }
    } else {
        vfs_node_t *dest_node = vfs_finddir(get_cwd(), argv[2]);
        if (dest_node && vfs_is_directory(dest_node)) {
            dest_dir = dest_node;
            strncpy(dest_name, src->name, VFS_MAX_NAME - 1);
        } else if (dest_node) {
            vga_puts("cp: destination exists: ");
            vga_puts(argv[2]);
            vga_puts("\n");
            return;
        } else {
            dest_dir = get_cwd();
            strncpy(dest_name, argv[2], VFS_MAX_NAME - 1);
        }
    }
    
    if (!dest_dir) {
        vga_puts("cp: destination directory not found\n");
        return;
    }
    
    if (vfs_finddir(dest_dir, dest_name)) {
        vga_puts("cp: destination exists: ");
        vga_puts(dest_name);
        vga_puts("\n");
        return;
    }
    
    if (vfs_create(dest_dir, dest_name, VFS_FILE) != 0) {
        vga_puts("cp: failed to create destination\n");
        return;
    }
    
    vfs_node_t *dest = vfs_finddir(dest_dir, dest_name);
    if (!dest) {
        vga_puts("cp: failed to find new file\n");
        return;
    }
    
    if (src->length > 0) {
        uint8_t *buffer = kmalloc(src->length);
        if (!buffer) {
            vga_puts("cp: out of memory\n");
            return;
        }
        
        int bytes_read = vfs_read(src, 0, src->length, buffer);
        if (bytes_read > 0) {
            vfs_write(dest, 0, bytes_read, buffer);
        }
        kfree(buffer);
    }
    
    vga_puts("Copied ");
    vga_puts(src->name);
    vga_puts(" -> ");
    vga_puts(dest_name);
    vga_puts("\n");
}

void cmd_mv(int argc, char **argv)
{
    if (argc < 3) {
        vga_puts("Usage: mv <source> <dest>\n");
        vga_puts("  dest can be a directory or new filename\n");
        return;
    }
    
    vfs_node_t *src;
    vfs_node_t *src_parent;
    if (argv[1][0] == '/') {
        src = vfs_lookup(argv[1]);
        char path_copy[VFS_MAX_PATH];
        strncpy(path_copy, argv[1], VFS_MAX_PATH - 1);
        char *last_slash = NULL;
        for (char *p = path_copy; *p; p++) {
            if (*p == '/') last_slash = p;
        }
        if (last_slash && last_slash != path_copy) {
            *last_slash = '\0';
            src_parent = vfs_lookup(path_copy);
        } else {
            src_parent = vfs_get_root();
        }
    } else {
        src = vfs_finddir(get_cwd(), argv[1]);
        src_parent = get_cwd();
    }
    
    if (!src) {
        vga_puts("mv: source not found: ");
        vga_puts(argv[1]);
        vga_puts("\n");
        return;
    }
    
    vfs_node_t *dest_dir = NULL;
    char dest_name[VFS_MAX_NAME];
    
    if (argv[2][0] == '/') {
        vfs_node_t *dest_node = vfs_lookup(argv[2]);
        if (dest_node && vfs_is_directory(dest_node)) {
            dest_dir = dest_node;
            strncpy(dest_name, src->name, VFS_MAX_NAME - 1);
        } else if (dest_node) {
            vga_puts("mv: destination exists: ");
            vga_puts(argv[2]);
            vga_puts("\n");
            return;
        } else {
            char path_copy[VFS_MAX_PATH];
            strncpy(path_copy, argv[2], VFS_MAX_PATH - 1);
            
            char *last_slash = NULL;
            for (char *p = path_copy; *p; p++) {
                if (*p == '/') last_slash = p;
            }
            
            if (last_slash && last_slash != path_copy) {
                *last_slash = '\0';
                dest_dir = vfs_lookup(path_copy);
                strncpy(dest_name, last_slash + 1, VFS_MAX_NAME - 1);
            } else if (last_slash == path_copy) {
                dest_dir = vfs_get_root();
                strncpy(dest_name, last_slash + 1, VFS_MAX_NAME - 1);
            }
        }
    } else {
        vfs_node_t *dest_node = vfs_finddir(get_cwd(), argv[2]);
        if (dest_node && vfs_is_directory(dest_node)) {
            dest_dir = dest_node;
            strncpy(dest_name, src->name, VFS_MAX_NAME - 1);
        } else if (dest_node) {
            vga_puts("mv: destination exists: ");
            vga_puts(argv[2]);
            vga_puts("\n");
            return;
        } else {
            dest_dir = get_cwd();
            strncpy(dest_name, argv[2], VFS_MAX_NAME - 1);
        }
    }
    
    if (!dest_dir) {
        vga_puts("mv: destination directory not found\n");
        return;
    }
    
    if (vfs_finddir(dest_dir, dest_name)) {
        vga_puts("mv: destination exists: ");
        vga_puts(dest_name);
        vga_puts("\n");
        return;
    }
    
    if (dest_dir == src_parent) {
        strncpy(src->name, dest_name, VFS_MAX_NAME - 1);
        src->name[VFS_MAX_NAME - 1] = '\0';
        vga_puts("Renamed -> ");
        vga_puts(dest_name);
        vga_puts("\n");
        return;
    }
    
    if (!vfs_is_directory(src)) {
        if (vfs_create(dest_dir, dest_name, VFS_FILE) != 0) {
            vga_puts("mv: failed to create destination\n");
            return;
        }
        
        vfs_node_t *dest = vfs_finddir(dest_dir, dest_name);
        if (!dest) {
            vga_puts("mv: failed to find new file\n");
            return;
        }
        
        if (src->length > 0) {
            uint8_t *buffer = kmalloc(src->length);
            if (!buffer) {
                vga_puts("mv: out of memory\n");
                return;
            }
            
            int bytes_read = vfs_read(src, 0, src->length, buffer);
            if (bytes_read > 0) {
                vfs_write(dest, 0, bytes_read, buffer);
            }
            kfree(buffer);
        }
        
        vfs_unlink(src_parent, src->name);
        
        vga_puts("Moved -> ");
        vga_puts(dest_name);
        vga_puts("\n");
    } else {
        vga_puts("mv: cannot move directories between locations\n");
    }
}

void cmd_stat(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: stat <name>\n");
        return;
    }
    
    vfs_node_t *node = vfs_finddir(get_cwd(), argv[1]);
    if (!node) {
        vga_puts("stat: not found: ");
        vga_puts(argv[1]);
        vga_puts("\n");
        return;
    }
    
    vga_puts("  Name: ");
    vga_puts(node->name);
    vga_puts("\n");
    
    vga_puts("  Type: ");
    if (node->flags & VFS_DIRECTORY) {
        vga_puts("directory\n");
    } else {
        vga_puts("file\n");
    }
    
    vga_puts("  Size: ");
    vga_put_dec(node->length);
    vga_puts(" bytes\n");
    
    vga_puts("  Inode: ");
    vga_put_dec(node->inode);
    vga_puts("\n");
    
    vga_puts("  Created: ");
    vga_put_dec(node->ctime);
    vga_puts("s after boot\n");
    
    vga_puts("  Modified: ");
    vga_put_dec(node->mtime);
    vga_puts("s after boot\n");
    
    vga_puts("  Accessed: ");
    vga_put_dec(node->atime);
    vga_puts("s after boot\n");
}

static void tree_print_recursive(vfs_node_t *dir, int depth, char *prefix, int is_last __attribute__((unused)))
{
    if (!dir || depth > 10) return; 
    
    uint32_t index = 0;
    dirent_t *entry;
    
    int entry_count = 0;
    while ((entry = vfs_readdir(dir, entry_count)) != NULL) {
        if (strcmp(entry->name, ".") != 0 && strcmp(entry->name, "..") != 0) {
            entry_count++;
        }
    }
    
    int current = 0;
    index = 0;
    while ((entry = vfs_readdir(dir, index++)) != NULL) {
        if (strcmp(entry->name, ".") == 0 || strcmp(entry->name, "..") == 0) {
            continue;
        }
        
        current++;
        int last = (current == entry_count);
        
        vga_puts(prefix);
        
        if (last) {
            vga_puts("\\-- ");
        } else {
            vga_puts("|-- ");
        }
        
        vga_puts(entry->name);
        
        vfs_node_t *child = vfs_finddir(dir, entry->name);
        if (child && vfs_is_directory(child)) {
            vga_puts("/\n");
            
            char new_prefix[256];
            strcpy(new_prefix, prefix);
            if (last) {
                strcat(new_prefix, "    ");
            } else {
                strcat(new_prefix, "|   ");
            }
            
            tree_print_recursive(child, depth + 1, new_prefix, last);
        } else {
            vga_puts("\n");
        }
    }
}

void cmd_tree(int argc, char **argv)
{
    vfs_node_t *dir = get_cwd();
    
    if (argc >= 2) {
        if (argv[1][0] == '/') {
            dir = vfs_lookup(argv[1]);
        } else {
            dir = vfs_finddir(get_cwd(), argv[1]);
        }
    }
    
    if (!dir) {
        vga_puts("tree: directory not found\n");
        return;
    }
    
    if (!vfs_is_directory(dir)) {
        vga_puts("tree: not a directory\n");
        return;
    }
    
    vga_puts(".\n");
    
    char prefix[256] = "";
    tree_print_recursive(dir, 0, prefix, 0);
}

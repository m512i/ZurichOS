/* Shell Commands - Core Utilities
 * grep, find, wc, head, tail, sort, uniq, diff, tar
 */

#include <shell/builtins.h>
#include <kernel/shell.h>
#include <drivers/vga.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <string.h>

void cmd_utils(int argc, char **argv)
{
    (void)argc; (void)argv;

    uint8_t saved = vga_get_color();

    vga_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    vga_puts("Core Utilities\n");
    vga_setcolor(saved);

    vga_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_puts("  grep");
    vga_setcolor(saved);
    vga_puts("  [-i] [-n] [-c] <pattern> <file>\n");
    vga_puts("        Search for pattern in file. -i=ignore case, -n=line numbers, -c=count\n");
    vga_puts("        e.g.  grep -in hello myfile.txt\n\n");

    vga_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_puts("  find");
    vga_setcolor(saved);
    vga_puts("  [path] [-name <pattern>] [-type f|d]\n");
    vga_puts("        Recursive file search. Wildcards: *suffix, prefix*, *mid*\n");
    vga_puts("        e.g.  find / -name *.txt -type f\n\n");

    vga_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_puts("  wc");
    vga_setcolor(saved);
    vga_puts("    [-l] [-w] [-c] <file>\n");
    vga_puts("        Count lines, words, characters. Default shows all three.\n");
    vga_puts("        e.g.  wc -l myfile.txt\n\n");

    vga_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_puts("  head");
    vga_setcolor(saved);
    vga_puts("  [-n <count>] <file>\n");
    vga_puts("        Show first N lines (default 10).\n");
    vga_puts("        e.g.  head -n 5 myfile.txt\n\n");

    vga_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_puts("  tail");
    vga_setcolor(saved);
    vga_puts("  [-n <count>] <file>\n");
    vga_puts("        Show last N lines (default 10).\n");
    vga_puts("        e.g.  tail -n 3 myfile.txt\n\n");

    vga_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_puts("  sort");
    vga_setcolor(saved);
    vga_puts("  [-r] <file>\n");
    vga_puts("        Sort lines alphabetically. -r=reverse.\n");
    vga_puts("        e.g.  sort -r names.txt\n\n");

    vga_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_puts("  uniq");
    vga_setcolor(saved);
    vga_puts("  [-c] [-d] <file>\n");
    vga_puts("        Remove adjacent duplicates. -c=show counts, -d=dupes only.\n");
    vga_puts("        e.g.  uniq -c sorted.txt\n\n");

    vga_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_puts("  diff");
    vga_setcolor(saved);
    vga_puts("  <file1> <file2>\n");
    vga_puts("        Compare two files line by line (color-coded output).\n");
    vga_puts("        e.g.  diff old.txt new.txt\n\n");

    vga_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_puts("  tar");
    vga_setcolor(saved);
    vga_puts("   <list|create|extract> <archive> [files...]\n");
    vga_puts("        Archive files (ZTAR format).\n");
    vga_puts("        e.g.  tar create backup.tar a.txt b.txt\n");
    vga_puts("              tar list backup.tar\n");
    vga_puts("              tar extract backup.tar\n");
}


static vfs_node_t *resolve_path(const char *path)
{
    if (path[0] == '/') {
        return vfs_lookup(path);
    }
    return vfs_finddir(shell_get_cwd(), path);
}


static uint8_t *read_file_contents(vfs_node_t *file, int *out_size)
{
    if (!file || vfs_is_directory(file) || file->length == 0) {
        *out_size = 0;
        return NULL;
    }

    uint8_t *buf = kmalloc(file->length + 1);
    if (!buf) {
        *out_size = 0;
        return NULL;
    }

    int bytes = vfs_read(file, 0, file->length, buf);
    if (bytes <= 0) {
        kfree(buf);
        *out_size = 0;
        return NULL;
    }

    buf[bytes] = '\0';
    *out_size = bytes;
    return buf;
}


static int match_pattern(const char *line, const char *pattern, int ignore_case)
{
    if (!ignore_case) {
        return strstr(line, pattern) != NULL;
    }

    
    const char *l = line;
    int plen = (int)strlen(pattern);
    int llen = (int)strlen(line);

    for (int i = 0; i <= llen - plen; i++) {
        int match = 1;
        for (int j = 0; j < plen; j++) {
            char a = l[i + j];
            char b = pattern[j];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) { match = 0; break; }
        }
        if (match) return 1;
    }
    return 0;
}


void cmd_grep(int argc, char **argv)
{
    if (argc < 3) {
        vga_puts("Usage: grep [-i] [-n] [-c] <pattern> <file>\n");
        return;
    }

    int ignore_case = 0;
    int show_linenum = 0;
    int count_only = 0;
    int arg_idx = 1;

    while (arg_idx < argc && argv[arg_idx][0] == '-') {
        for (int i = 1; argv[arg_idx][i]; i++) {
            switch (argv[arg_idx][i]) {
                case 'i': ignore_case = 1; break;
                case 'n': show_linenum = 1; break;
                case 'c': count_only = 1; break;
                default:
                    vga_puts("grep: unknown option -");
                    vga_putchar(argv[arg_idx][i]);
                    vga_putchar('\n');
                    return;
            }
        }
        arg_idx++;
    }

    if (arg_idx + 1 >= argc) {
        vga_puts("Usage: grep [-i] [-n] [-c] <pattern> <file>\n");
        return;
    }

    const char *pattern = argv[arg_idx];
    const char *filepath = argv[arg_idx + 1];

    vfs_node_t *file = resolve_path(filepath);
    if (!file) {
        vga_puts("grep: file not found: ");
        vga_puts(filepath);
        vga_putchar('\n');
        return;
    }

    if (vfs_is_directory(file)) {
        vga_puts("grep: is a directory: ");
        vga_puts(filepath);
        vga_putchar('\n');
        return;
    }

    int size;
    uint8_t *data = read_file_contents(file, &size);
    if (!data) {
        vga_puts("grep: cannot read file\n");
        return;
    }

    int line_num = 1;
    int match_count = 0;
    char *line_start = (char *)data;

    while (*line_start) {
        char *line_end = strchr(line_start, '\n');
        if (line_end) {
            *line_end = '\0';
        }

        if (match_pattern(line_start, pattern, ignore_case)) {
            match_count++;
            if (!count_only) {
                if (show_linenum) {
                    vga_put_dec(line_num);
                    vga_puts(": ");
                }
                vga_puts(line_start);
                vga_putchar('\n');
            }
        }

        if (line_end) {
            line_start = line_end + 1;
        } else {
            break;
        }
        line_num++;
    }

    if (count_only) {
        vga_put_dec(match_count);
        vga_putchar('\n');
    }

    kfree(data);
}


static int find_name_match(const char *name, const char *pattern)
{
    
    int plen = (int)strlen(pattern);
    int nlen = (int)strlen(name);

    if (plen == 0) return 1;

    
    if (strchr(pattern, '*') == NULL) {
        return strcmp(name, pattern) == 0;
    }

    
    if (pattern[0] == '*' && strchr(pattern + 1, '*') == NULL) {
        const char *suffix = pattern + 1;
        int slen = (int)strlen(suffix);
        if (nlen < slen) return 0;
        return strcmp(name + nlen - slen, suffix) == 0;
    }

    
    if (pattern[plen - 1] == '*' && strchr(pattern, '*') == pattern + plen - 1) {
        return strncmp(name, pattern, plen - 1) == 0;
    }

    
    if (pattern[0] == '*' && pattern[plen - 1] == '*') {
        char inner[VFS_MAX_NAME];
        strncpy(inner, pattern + 1, plen - 2);
        inner[plen - 2] = '\0';
        return strstr(name, inner) != NULL;
    }

    
    return strstr(name, pattern) != NULL;
}

static void find_recursive(vfs_node_t *dir, const char *prefix,
                           const char *name_pattern, int type_filter, int *count)
{
    uint32_t index = 0;
    dirent_t *entry;

    while ((entry = vfs_readdir(dir, index++)) != NULL) {
        if (strcmp(entry->name, ".") == 0 || strcmp(entry->name, "..") == 0)
            continue;

        vfs_node_t *child = vfs_finddir(dir, entry->name);
        if (!child) continue;

        int is_dir = vfs_is_directory(child);

        
        char path[VFS_MAX_PATH];
        if (prefix[0] == '/' && prefix[1] == '\0') {
            strcpy(path, "/");
            strcat(path, entry->name);
        } else {
            strcpy(path, prefix);
            strcat(path, "/");
            strcat(path, entry->name);
        }

        
        int type_ok = 1;
        if (type_filter == 'f' && is_dir) type_ok = 0;
        if (type_filter == 'd' && !is_dir) type_ok = 0;

        int name_ok = 1;
        if (name_pattern) {
            name_ok = find_name_match(entry->name, name_pattern);
        }

        if (type_ok && name_ok) {
            vga_puts(path);
            vga_putchar('\n');
            (*count)++;
        }

        if (is_dir) {
            find_recursive(child, path, name_pattern, type_filter, count);
        }
    }
}

void cmd_find(int argc, char **argv)
{
    const char *search_path = ".";
    const char *name_pattern = NULL;
    int type_filter = 0;
    int arg_idx = 1;

    
    if (arg_idx < argc && argv[arg_idx][0] != '-') {
        search_path = argv[arg_idx];
        arg_idx++;
    }

    
    while (arg_idx < argc) {
        if (strcmp(argv[arg_idx], "-name") == 0 && arg_idx + 1 < argc) {
            name_pattern = argv[arg_idx + 1];
            arg_idx += 2;
        } else if (strcmp(argv[arg_idx], "-type") == 0 && arg_idx + 1 < argc) {
            type_filter = argv[arg_idx + 1][0];
            arg_idx += 2;
        } else {
            vga_puts("find: unknown option: ");
            vga_puts(argv[arg_idx]);
            vga_putchar('\n');
            return;
        }
    }

    vfs_node_t *dir;
    if (strcmp(search_path, ".") == 0) {
        dir = shell_get_cwd();
    } else {
        dir = resolve_path(search_path);
    }

    if (!dir || !vfs_is_directory(dir)) {
        vga_puts("find: not a directory: ");
        vga_puts(search_path);
        vga_putchar('\n');
        return;
    }

    
    char prefix[VFS_MAX_PATH];
    if (strcmp(search_path, ".") == 0) {
        strcpy(prefix, ".");
    } else {
        strncpy(prefix, search_path, VFS_MAX_PATH - 1);
        prefix[VFS_MAX_PATH - 1] = '\0';
    }

    int count = 0;
    find_recursive(dir, prefix, name_pattern, type_filter, &count);

    if (count == 0) {
        vga_puts("(no matches)\n");
    }
}


void cmd_wc(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: wc [-l] [-w] [-c] <file>\n");
        return;
    }

    int show_lines = 0, show_words = 0, show_chars = 0;
    int arg_idx = 1;

    while (arg_idx < argc && argv[arg_idx][0] == '-') {
        for (int i = 1; argv[arg_idx][i]; i++) {
            switch (argv[arg_idx][i]) {
                case 'l': show_lines = 1; break;
                case 'w': show_words = 1; break;
                case 'c': show_chars = 1; break;
                default:
                    vga_puts("wc: unknown option -");
                    vga_putchar(argv[arg_idx][i]);
                    vga_putchar('\n');
                    return;
            }
        }
        arg_idx++;
    }

    
    if (!show_lines && !show_words && !show_chars) {
        show_lines = show_words = show_chars = 1;
    }

    if (arg_idx >= argc) {
        vga_puts("Usage: wc [-l] [-w] [-c] <file>\n");
        return;
    }

    const char *filepath = argv[arg_idx];
    vfs_node_t *file = resolve_path(filepath);
    if (!file) {
        vga_puts("wc: file not found: ");
        vga_puts(filepath);
        vga_putchar('\n');
        return;
    }

    if (vfs_is_directory(file)) {
        vga_puts("wc: is a directory: ");
        vga_puts(filepath);
        vga_putchar('\n');
        return;
    }

    int size;
    uint8_t *data = read_file_contents(file, &size);
    if (!data && size == 0 && file->length == 0) {
        
        if (show_lines) { vga_puts("  0"); }
        if (show_words) { vga_puts("  0"); }
        if (show_chars) { vga_puts("  0"); }
        vga_puts(" ");
        vga_puts(filepath);
        vga_putchar('\n');
        return;
    }
    if (!data) {
        vga_puts("wc: cannot read file\n");
        return;
    }

    uint32_t lines = 0, words = 0, chars = (uint32_t)size;
    int in_word = 0;

    for (int i = 0; i < size; i++) {
        if (data[i] == '\n') lines++;
        if (data[i] == ' ' || data[i] == '\t' || data[i] == '\n' || data[i] == '\r') {
            in_word = 0;
        } else {
            if (!in_word) words++;
            in_word = 1;
        }
    }

    
    if (size > 0 && data[size - 1] != '\n') lines++;

    if (show_lines) { vga_puts("  "); vga_put_dec(lines); }
    if (show_words) { vga_puts("  "); vga_put_dec(words); }
    if (show_chars) { vga_puts("  "); vga_put_dec(chars); }
    vga_puts(" ");
    vga_puts(filepath);
    vga_putchar('\n');

    kfree(data);
}


void cmd_head(int argc, char **argv)
{
    int num_lines = 10;
    int arg_idx = 1;

    if (arg_idx < argc && strcmp(argv[arg_idx], "-n") == 0) {
        if (arg_idx + 1 < argc) {
            num_lines = (int)shell_parse_dec(argv[arg_idx + 1]);
            if (num_lines <= 0) num_lines = 10;
            arg_idx += 2;
        } else {
            vga_puts("head: -n requires a number\n");
            return;
        }
    }

    if (arg_idx >= argc) {
        vga_puts("Usage: head [-n <count>] <file>\n");
        return;
    }

    vfs_node_t *file = resolve_path(argv[arg_idx]);
    if (!file) {
        vga_puts("head: file not found: ");
        vga_puts(argv[arg_idx]);
        vga_putchar('\n');
        return;
    }

    if (vfs_is_directory(file)) {
        vga_puts("head: is a directory\n");
        return;
    }

    int size;
    uint8_t *data = read_file_contents(file, &size);
    if (!data) {
        if (file->length == 0) return;
        vga_puts("head: cannot read file\n");
        return;
    }

    int lines_printed = 0;
    char *p = (char *)data;
    while (*p && lines_printed < num_lines) {
        char *nl = strchr(p, '\n');
        if (nl) {
            *nl = '\0';
            vga_puts(p);
            vga_putchar('\n');
            p = nl + 1;
        } else {
            vga_puts(p);
            vga_putchar('\n');
            break;
        }
        lines_printed++;
    }

    kfree(data);
}


void cmd_tail(int argc, char **argv)
{
    int num_lines = 10;
    int arg_idx = 1;

    if (arg_idx < argc && strcmp(argv[arg_idx], "-n") == 0) {
        if (arg_idx + 1 < argc) {
            num_lines = (int)shell_parse_dec(argv[arg_idx + 1]);
            if (num_lines <= 0) num_lines = 10;
            arg_idx += 2;
        } else {
            vga_puts("tail: -n requires a number\n");
            return;
        }
    }

    if (arg_idx >= argc) {
        vga_puts("Usage: tail [-n <count>] <file>\n");
        return;
    }

    vfs_node_t *file = resolve_path(argv[arg_idx]);
    if (!file) {
        vga_puts("tail: file not found: ");
        vga_puts(argv[arg_idx]);
        vga_putchar('\n');
        return;
    }

    if (vfs_is_directory(file)) {
        vga_puts("tail: is a directory\n");
        return;
    }

    int size;
    uint8_t *data = read_file_contents(file, &size);
    if (!data) {
        if (file->length == 0) return;
        vga_puts("tail: cannot read file\n");
        return;
    }

    
    int total_lines = 0;
    for (int i = 0; i < size; i++) {
        if (data[i] == '\n') total_lines++;
    }
    if (size > 0 && data[size - 1] != '\n') total_lines++;

    int skip = total_lines - num_lines;
    if (skip < 0) skip = 0;

    int current_line = 0;
    char *p = (char *)data;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = '\0';

        if (current_line >= skip) {
            vga_puts(p);
            vga_putchar('\n');
        }

        if (nl) {
            p = nl + 1;
        } else {
            break;
        }
        current_line++;
    }

    kfree(data);
}


void cmd_sort(int argc, char **argv)
{
    int reverse = 0;
    int arg_idx = 1;

    if (arg_idx < argc && strcmp(argv[arg_idx], "-r") == 0) {
        reverse = 1;
        arg_idx++;
    }

    if (arg_idx >= argc) {
        vga_puts("Usage: sort [-r] <file>\n");
        return;
    }

    vfs_node_t *file = resolve_path(argv[arg_idx]);
    if (!file) {
        vga_puts("sort: file not found: ");
        vga_puts(argv[arg_idx]);
        vga_putchar('\n');
        return;
    }

    if (vfs_is_directory(file)) {
        vga_puts("sort: is a directory\n");
        return;
    }

    int size;
    uint8_t *data = read_file_contents(file, &size);
    if (!data) {
        if (file->length == 0) return;
        vga_puts("sort: cannot read file\n");
        return;
    }

    
    int num_lines = 0;
    for (int i = 0; i < size; i++) {
        if (data[i] == '\n') num_lines++;
    }
    if (size > 0 && data[size - 1] != '\n') num_lines++;

    if (num_lines == 0) {
        kfree(data);
        return;
    }

    
    char **lines = kmalloc(num_lines * sizeof(char *));
    if (!lines) {
        vga_puts("sort: out of memory\n");
        kfree(data);
        return;
    }

    int idx = 0;
    char *p = (char *)data;
    while (*p && idx < num_lines) {
        lines[idx++] = p;
        char *nl = strchr(p, '\n');
        if (nl) {
            *nl = '\0';
            p = nl + 1;
        } else {
            break;
        }
    }
    num_lines = idx;

    
    for (int i = 0; i < num_lines - 1; i++) {
        for (int j = 0; j < num_lines - 1 - i; j++) {
            int cmp = strcmp(lines[j], lines[j + 1]);
            if (reverse) cmp = -cmp;
            if (cmp > 0) {
                char *tmp = lines[j];
                lines[j] = lines[j + 1];
                lines[j + 1] = tmp;
            }
        }
    }

    
    for (int i = 0; i < num_lines; i++) {
        vga_puts(lines[i]);
        vga_putchar('\n');
    }

    kfree(lines);
    kfree(data);
}


void cmd_uniq(int argc, char **argv)
{
    int show_count = 0;
    int dupes_only = 0;
    int arg_idx = 1;

    while (arg_idx < argc && argv[arg_idx][0] == '-') {
        for (int i = 1; argv[arg_idx][i]; i++) {
            switch (argv[arg_idx][i]) {
                case 'c': show_count = 1; break;
                case 'd': dupes_only = 1; break;
                default:
                    vga_puts("uniq: unknown option -");
                    vga_putchar(argv[arg_idx][i]);
                    vga_putchar('\n');
                    return;
            }
        }
        arg_idx++;
    }

    if (arg_idx >= argc) {
        vga_puts("Usage: uniq [-c] [-d] <file>\n");
        return;
    }

    vfs_node_t *file = resolve_path(argv[arg_idx]);
    if (!file) {
        vga_puts("uniq: file not found: ");
        vga_puts(argv[arg_idx]);
        vga_putchar('\n');
        return;
    }

    if (vfs_is_directory(file)) {
        vga_puts("uniq: is a directory\n");
        return;
    }

    int size;
    uint8_t *data = read_file_contents(file, &size);
    if (!data) {
        if (file->length == 0) return;
        vga_puts("uniq: cannot read file\n");
        return;
    }

    char *prev_line = NULL;
    int count = 0;
    char *p = (char *)data;

    while (1) {
        char *line_start = p;
        char *nl = NULL;
        int at_end = 0;

        if (*p == '\0') break;

        nl = strchr(p, '\n');
        if (nl) {
            *nl = '\0';
            p = nl + 1;
        } else {
            at_end = 1;
            p = line_start + strlen(line_start);
        }

        if (prev_line && strcmp(prev_line, line_start) == 0) {
            count++;
        } else {
            
            if (prev_line) {
                if (!dupes_only || count > 1) {
                    if (show_count) {
                        vga_puts("  ");
                        vga_put_dec(count);
                        vga_puts(" ");
                    }
                    vga_puts(prev_line);
                    vga_putchar('\n');
                }
            }
            prev_line = line_start;
            count = 1;
        }

        if (at_end) break;
    }

    
    if (prev_line) {
        if (!dupes_only || count > 1) {
            if (show_count) {
                vga_puts("  ");
                vga_put_dec(count);
                vga_puts(" ");
            }
            vga_puts(prev_line);
            vga_putchar('\n');
        }
    }

    kfree(data);
}


void cmd_diff(int argc, char **argv)
{
    if (argc < 3) {
        vga_puts("Usage: diff <file1> <file2>\n");
        return;
    }

    vfs_node_t *node1 = resolve_path(argv[1]);
    vfs_node_t *node2 = resolve_path(argv[2]);

    if (!node1) {
        vga_puts("diff: file not found: ");
        vga_puts(argv[1]);
        vga_putchar('\n');
        return;
    }
    if (!node2) {
        vga_puts("diff: file not found: ");
        vga_puts(argv[2]);
        vga_putchar('\n');
        return;
    }

    int size1, size2;
    uint8_t *data1 = read_file_contents(node1, &size1);
    uint8_t *data2 = read_file_contents(node2, &size2);

    
    if (!data1 && node1->length > 0) {
        vga_puts("diff: cannot read: ");
        vga_puts(argv[1]);
        vga_putchar('\n');
        if (data2) kfree(data2);
        return;
    }
    if (!data2 && node2->length > 0) {
        vga_puts("diff: cannot read: ");
        vga_puts(argv[2]);
        vga_putchar('\n');
        if (data1) kfree(data1);
        return;
    }

    
    int max_lines = 4096;

    
    int nlines1 = 0;
    if (data1) {
        for (int i = 0; i < size1; i++)
            if (data1[i] == '\n') nlines1++;
        if (size1 > 0 && data1[size1 - 1] != '\n') nlines1++;
    }

    
    int nlines2 = 0;
    if (data2) {
        for (int i = 0; i < size2; i++)
            if (data2[i] == '\n') nlines2++;
        if (size2 > 0 && data2[size2 - 1] != '\n') nlines2++;
    }

    if (nlines1 > max_lines) nlines1 = max_lines;
    if (nlines2 > max_lines) nlines2 = max_lines;

    char **lines1 = NULL;
    char **lines2 = NULL;

    if (nlines1 > 0) {
        lines1 = kmalloc(nlines1 * sizeof(char *));
        if (!lines1) {
            vga_puts("diff: out of memory\n");
            if (data1) kfree(data1);
            if (data2) kfree(data2);
            return;
        }
        int idx = 0;
        char *p = (char *)data1;
        while (*p && idx < nlines1) {
            lines1[idx++] = p;
            char *nl = strchr(p, '\n');
            if (nl) { *nl = '\0'; p = nl + 1; }
            else break;
        }
        nlines1 = idx;
    }

    if (nlines2 > 0) {
        lines2 = kmalloc(nlines2 * sizeof(char *));
        if (!lines2) {
            vga_puts("diff: out of memory\n");
            if (lines1) kfree(lines1);
            if (data1) kfree(data1);
            if (data2) kfree(data2);
            return;
        }
        int idx = 0;
        char *p = (char *)data2;
        while (*p && idx < nlines2) {
            lines2[idx++] = p;
            char *nl = strchr(p, '\n');
            if (nl) { *nl = '\0'; p = nl + 1; }
            else break;
        }
        nlines2 = idx;
    }

    
    int max = nlines1 > nlines2 ? nlines1 : nlines2;
    int diffs = 0;
    uint8_t saved_color = vga_get_color();

    for (int i = 0; i < max; i++) {
        const char *l1 = (i < nlines1) ? lines1[i] : NULL;
        const char *l2 = (i < nlines2) ? lines2[i] : NULL;

        if (l1 && l2 && strcmp(l1, l2) == 0) continue;

        diffs++;

        vga_put_dec(i + 1);
        if (l1 && !l2) {
            vga_puts("d");
            vga_put_dec(i + 1);
            vga_putchar('\n');
            vga_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
            vga_puts("< ");
            vga_puts(l1);
            vga_putchar('\n');
            vga_setcolor(saved_color);
        } else if (!l1 && l2) {
            vga_puts("a");
            vga_put_dec(i + 1);
            vga_putchar('\n');
            vga_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
            vga_puts("> ");
            vga_puts(l2);
            vga_putchar('\n');
            vga_setcolor(saved_color);
        } else {
            vga_puts("c");
            vga_put_dec(i + 1);
            vga_putchar('\n');
            vga_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
            vga_puts("< ");
            vga_puts(l1);
            vga_putchar('\n');
            vga_setcolor(saved_color);
            vga_puts("---\n");
            vga_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
            vga_puts("> ");
            vga_puts(l2);
            vga_putchar('\n');
            vga_setcolor(saved_color);
        }
    }

    if (diffs == 0) {
        vga_puts("Files are identical\n");
    }

    if (lines1) kfree(lines1);
    if (lines2) kfree(lines2);
    if (data1) kfree(data1);
    if (data2) kfree(data2);
}



#define TAR_MAGIC "ZTAR"
#define TAR_MAGIC_LEN 4

static void tar_write_u32(uint8_t *buf, uint32_t val)
{
    buf[0] = (val >> 0) & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
    buf[3] = (val >> 24) & 0xFF;
}

static uint32_t tar_read_u32(const uint8_t *buf)
{
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

void cmd_tar(int argc, char **argv)
{
    if (argc < 3) {
        vga_puts("Usage: tar <list|create|extract> <archive> [files...]\n");
        return;
    }

    const char *action = argv[1];
    const char *archive_path = argv[2];

    if (strcmp(action, "list") == 0) {
        
        vfs_node_t *arc = resolve_path(archive_path);
        if (!arc) {
            vga_puts("tar: archive not found: ");
            vga_puts(archive_path);
            vga_putchar('\n');
            return;
        }

        int size;
        uint8_t *data = read_file_contents(arc, &size);
        if (!data) {
            vga_puts("tar: cannot read archive\n");
            return;
        }

        if (size < TAR_MAGIC_LEN || memcmp(data, TAR_MAGIC, TAR_MAGIC_LEN) != 0) {
            vga_puts("tar: not a valid archive\n");
            kfree(data);
            return;
        }

        int offset = TAR_MAGIC_LEN;
        int entry_num = 0;

        while (offset + 4 <= size) {
            uint32_t name_len = tar_read_u32(data + offset);
            offset += 4;
            if (offset + (int)name_len + 4 > size) break;

            char name[VFS_MAX_NAME];
            uint32_t copy_len = name_len < VFS_MAX_NAME - 1 ? name_len : VFS_MAX_NAME - 1;
            memcpy(name, data + offset, copy_len);
            name[copy_len] = '\0';
            offset += name_len;

            uint32_t data_len = tar_read_u32(data + offset);
            offset += 4;

            vga_puts("  ");
            vga_puts(name);
            vga_puts("  (");
            vga_put_dec(data_len);
            vga_puts(" bytes)\n");

            offset += data_len;
            entry_num++;
        }

        vga_put_dec(entry_num);
        vga_puts(" file(s) in archive\n");
        kfree(data);

    } else if (strcmp(action, "create") == 0) {
        if (argc < 4) {
            vga_puts("Usage: tar create <archive> <file1> [file2] ...\n");
            return;
        }

        
        uint32_t total = TAR_MAGIC_LEN;
        int num_files = argc - 3;

        for (int i = 0; i < num_files; i++) {
            vfs_node_t *f = resolve_path(argv[3 + i]);
            if (!f || vfs_is_directory(f)) {
                vga_puts("tar: skipping ");
                vga_puts(argv[3 + i]);
                vga_puts(" (not a file)\n");
                continue;
            }
            total += 4 + strlen(argv[3 + i]) + 4 + f->length;
        }

        uint8_t *archive = kmalloc(total);
        if (!archive) {
            vga_puts("tar: out of memory\n");
            return;
        }

        memcpy(archive, TAR_MAGIC, TAR_MAGIC_LEN);
        uint32_t offset = TAR_MAGIC_LEN;
        int packed = 0;

        for (int i = 0; i < num_files; i++) {
            vfs_node_t *f = resolve_path(argv[3 + i]);
            if (!f || vfs_is_directory(f)) continue;

            uint32_t name_len = strlen(argv[3 + i]);
            tar_write_u32(archive + offset, name_len);
            offset += 4;
            memcpy(archive + offset, argv[3 + i], name_len);
            offset += name_len;

            tar_write_u32(archive + offset, f->length);
            offset += 4;

            if (f->length > 0) {
                vfs_read(f, 0, f->length, archive + offset);
                offset += f->length;
            }
            packed++;
        }

        
        vfs_node_t *parent = shell_get_cwd();
        const char *arc_name = archive_path;

        
        if (archive_path[0] == '/') {
            
            char path_copy[VFS_MAX_PATH];
            strncpy(path_copy, archive_path, VFS_MAX_PATH - 1);
            path_copy[VFS_MAX_PATH - 1] = '\0';
            char *last_slash = strrchr(path_copy, '/');
            if (last_slash && last_slash != path_copy) {
                *last_slash = '\0';
                arc_name = last_slash + 1;
                parent = vfs_lookup(path_copy);
            } else if (last_slash == path_copy) {
                arc_name = path_copy + 1;
                parent = vfs_get_root();
            }
        }

        if (!parent) {
            vga_puts("tar: cannot find parent directory\n");
            kfree(archive);
            return;
        }

        
        vfs_node_t *existing = vfs_finddir(parent, arc_name);
        if (!existing) {
            vfs_create(parent, arc_name, VFS_FILE);
            existing = vfs_finddir(parent, arc_name);
        }
        if (!existing) {
            vga_puts("tar: cannot create archive file\n");
            kfree(archive);
            return;
        }

        vfs_truncate(existing);
        vfs_write(existing, 0, offset, archive);

        vga_puts("Created archive: ");
        vga_puts(archive_path);
        vga_puts(" (");
        vga_put_dec(packed);
        vga_puts(" files, ");
        vga_put_dec(offset);
        vga_puts(" bytes)\n");

        kfree(archive);

    } else if (strcmp(action, "extract") == 0) {
        vfs_node_t *arc = resolve_path(archive_path);
        if (!arc) {
            vga_puts("tar: archive not found: ");
            vga_puts(archive_path);
            vga_putchar('\n');
            return;
        }

        int size;
        uint8_t *data = read_file_contents(arc, &size);
        if (!data) {
            vga_puts("tar: cannot read archive\n");
            return;
        }

        if (size < TAR_MAGIC_LEN || memcmp(data, TAR_MAGIC, TAR_MAGIC_LEN) != 0) {
            vga_puts("tar: not a valid archive\n");
            kfree(data);
            return;
        }

        vfs_node_t *cwd = shell_get_cwd();
        int offset = TAR_MAGIC_LEN;
        int extracted = 0;

        while (offset + 4 <= size) {
            uint32_t name_len = tar_read_u32(data + offset);
            offset += 4;
            if (offset + (int)name_len + 4 > size) break;

            char name[VFS_MAX_NAME];
            uint32_t copy_len = name_len < VFS_MAX_NAME - 1 ? name_len : VFS_MAX_NAME - 1;
            memcpy(name, data + offset, copy_len);
            name[copy_len] = '\0';
            offset += name_len;

            uint32_t data_len = tar_read_u32(data + offset);
            offset += 4;
            if (offset + (int)data_len > size) break;

            
            vfs_node_t *existing = vfs_finddir(cwd, name);
            if (!existing) {
                vfs_create(cwd, name, VFS_FILE);
                existing = vfs_finddir(cwd, name);
            }
            if (existing) {
                vfs_truncate(existing);
                if (data_len > 0) {
                    vfs_write(existing, 0, data_len, data + offset);
                }
                vga_puts("  ");
                vga_puts(name);
                vga_puts(" (");
                vga_put_dec(data_len);
                vga_puts(" bytes)\n");
                extracted++;
            }

            offset += data_len;
        }

        vga_put_dec(extracted);
        vga_puts(" file(s) extracted\n");
        kfree(data);

    } else {
        vga_puts("tar: unknown action: ");
        vga_puts(action);
        vga_puts("\nUsage: tar <list|create|extract> <archive> [files...]\n");
    }
}

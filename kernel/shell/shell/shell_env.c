/* Shell Environment Variables
 * $PATH, $HOME, $USER, $PWD, etc.
 */

#include <shell/shell_features.h>
#include <kernel/shell.h>
#include <drivers/vga.h>
#include <drivers/serial.h>
#include <string.h>

#define MAX_ENV_VARS    32
#define MAX_ENV_KEY     32
#define MAX_ENV_VALUE   128

typedef struct {
    char key[MAX_ENV_KEY];
    char value[MAX_ENV_VALUE];
    int in_use;
} env_var_t;

static env_var_t env_vars[MAX_ENV_VARS];

void env_init(void)
{
    memset(env_vars, 0, sizeof(env_vars));
    
    env_set("PATH", "/bin:/usr/bin:/sbin");
    env_set("HOME", "/root");
    env_set("USER", "root");
    env_set("SHELL", "/bin/sh");
    env_set("HOSTNAME", "zurich");
    env_set("TERM", "vga");
    env_set("PS1", "zurich:$PWD> ");
    env_set("PWD", "/");
    
    serial_puts("[SHELL] Environment initialized\n");
}

int env_set(const char *key, const char *value)
{
    if (!key || !value) return -1;
    
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (env_vars[i].in_use && strcmp(env_vars[i].key, key) == 0) {
            strncpy(env_vars[i].value, value, MAX_ENV_VALUE - 1);
            env_vars[i].value[MAX_ENV_VALUE - 1] = '\0';
            return 0;
        }
    }
    
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (!env_vars[i].in_use) {
            strncpy(env_vars[i].key, key, MAX_ENV_KEY - 1);
            env_vars[i].key[MAX_ENV_KEY - 1] = '\0';
            strncpy(env_vars[i].value, value, MAX_ENV_VALUE - 1);
            env_vars[i].value[MAX_ENV_VALUE - 1] = '\0';
            env_vars[i].in_use = 1;
            return 0;
        }
    }
    
    return -1;
}

const char *env_get(const char *key)
{
    if (!key) return NULL;
    
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (env_vars[i].in_use && strcmp(env_vars[i].key, key) == 0) {
            return env_vars[i].value;
        }
    }
    
    return NULL;
}

int env_unset(const char *key)
{
    if (!key) return -1;
    
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (env_vars[i].in_use && strcmp(env_vars[i].key, key) == 0) {
            env_vars[i].in_use = 0;
            return 0;
        }
    }
    
    return -1;
}

int env_expand(const char *input, char *output, int max_len)
{
    const char *src = input;
    char *dst = output;
    char *end = output + max_len - 1;
    
    while (*src && dst < end) {
        if (*src == '$') {
            src++;
            if (*src == '{') {
                src++;
                char varname[MAX_ENV_KEY];
                int vi = 0;
                while (*src && *src != '}' && vi < MAX_ENV_KEY - 1) {
                    varname[vi++] = *src++;
                }
                varname[vi] = '\0';
                if (*src == '}') src++;
                
                const char *val = env_get(varname);
                if (val) {
                    while (*val && dst < end) {
                        *dst++ = *val++;
                    }
                }
            } else if (*src == '?') {
                *dst++ = '0';
                src++;
            } else if (*src == '$') {
                *dst++ = '1';
                src++;
            } else {
                char varname[MAX_ENV_KEY];
                int vi = 0;
                while (*src && ((*src >= 'A' && *src <= 'Z') || 
                       (*src >= 'a' && *src <= 'z') ||
                       (*src >= '0' && *src <= '9') || *src == '_') && 
                       vi < MAX_ENV_KEY - 1) {
                    varname[vi++] = *src++;
                }
                varname[vi] = '\0';
                
                if (vi > 0) {
                    const char *val = env_get(varname);
                    if (val) {
                        while (*val && dst < end) {
                            *dst++ = *val++;
                        }
                    }
                } else {
                    *dst++ = '$';
                }
            }
        } else {
            *dst++ = *src++;
        }
    }
    
    *dst = '\0';
    return (int)(dst - output);
}

void env_list(void)
{
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (env_vars[i].in_use) {
            vga_puts(env_vars[i].key);
            vga_puts("=");
            vga_puts(env_vars[i].value);
            vga_puts("\n");
        }
    }
}

int env_count(void)
{
    int count = 0;
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (env_vars[i].in_use) count++;
    }
    return count;
}

#ifndef _SHELL_BUILTINS_H
#define _SHELL_BUILTINS_H

#include <stdint.h>

typedef void (*cmd_handler_t)(int argc, char **argv);
typedef struct {
    const char *name;
    const char *description;
    cmd_handler_t handler;
} shell_command_t;

uint32_t shell_parse_hex(const char *str);
uint32_t shell_parse_dec(const char *str);
extern shell_command_t shell_commands[];

void cmd_help(int argc, char **argv);
void cmd_clear(int argc, char **argv);
void cmd_echo(int argc, char **argv);
void cmd_mem(int argc, char **argv);
void cmd_free(int argc, char **argv);
void cmd_reboot(int argc, char **argv);
void cmd_halt(int argc, char **argv);
void cmd_version(int argc, char **argv);
void cmd_uptime(int argc, char **argv);
void cmd_color(int argc, char **argv);
void cmd_hexdump(int argc, char **argv);
void cmd_peek(int argc, char **argv);
void cmd_poke(int argc, char **argv);
void cmd_alloc(int argc, char **argv);
void cmd_symbols(int argc, char **argv);
void cmd_time(int argc, char **argv);
void cmd_date(int argc, char **argv);
void cmd_timezone(int argc, char **argv);
void cmd_lspci(int argc, char **argv);
void cmd_panic(int argc, char **argv);
void cmd_beep(int argc, char **argv);
void cmd_play(int argc, char **argv);
void cmd_exit(int argc, char **argv);
void cmd_vga(int argc, char **argv);

void cmd_ls(int argc, char **argv);
void cmd_cd(int argc, char **argv);
void cmd_pwd(int argc, char **argv);
void cmd_cat(int argc, char **argv);
void cmd_touch(int argc, char **argv);
void cmd_mkdir(int argc, char **argv);
void cmd_rmdir(int argc, char **argv);
void cmd_rm(int argc, char **argv);
void cmd_write(int argc, char **argv);
void cmd_append(int argc, char **argv);
void cmd_cp(int argc, char **argv);
void cmd_mv(int argc, char **argv);
void cmd_stat(int argc, char **argv);
void cmd_tree(int argc, char **argv);

void cmd_ps(int argc, char **argv);
void cmd_kill(int argc, char **argv);

void cmd_lsblk(int argc, char **argv);
void cmd_hdinfo(int argc, char **argv);
void cmd_readsec(int argc, char **argv);

void cmd_fatmount(int argc, char **argv);
void cmd_fatls(int argc, char **argv);
void cmd_fatcat(int argc, char **argv);
void cmd_mounts(int argc, char **argv);

void cmd_apic(int argc, char **argv);
void cmd_drivers(int argc, char **argv);

void cmd_memtest(int argc, char **argv);
void cmd_heapstats(int argc, char **argv);
void cmd_leaktest(int argc, char **argv);

void cmd_tasks(int argc, char **argv);

void cmd_exec(int argc, char **argv);

void cmd_synctest(int argc, char **argv);
void cmd_pritest(int argc, char **argv);
void cmd_cvtest(int argc, char **argv);
void cmd_rwtest(int argc, char **argv);
void cmd_asserttest(int argc, char **argv);
void cmd_guardtest(int argc, char **argv);

void cmd_netinit(int argc, char **argv);
void cmd_ifconfig(int argc, char **argv);
void cmd_ping(int argc, char **argv);
void cmd_arp(int argc, char **argv);
void cmd_netpoll(int argc, char **argv);
void cmd_netstat(int argc, char **argv);
void cmd_dhcp(int argc, char **argv);
void cmd_dns(int argc, char **argv);
void cmd_route(int argc, char **argv);

void cmd_isolation(int argc, char **argv);

void cmd_utils(int argc, char **argv);
void cmd_grep(int argc, char **argv);
void cmd_find(int argc, char **argv);
void cmd_wc(int argc, char **argv);
void cmd_head(int argc, char **argv);
void cmd_tail(int argc, char **argv);
void cmd_sort(int argc, char **argv);
void cmd_uniq(int argc, char **argv);
void cmd_diff(int argc, char **argv);
void cmd_tar(int argc, char **argv);

void cmd_export(int argc, char **argv);
void cmd_unset(int argc, char **argv);
void cmd_env(int argc, char **argv);
void cmd_set(int argc, char **argv);
void cmd_source(int argc, char **argv);
void cmd_jobs(int argc, char **argv);
void cmd_fg(int argc, char **argv);
void cmd_history(int argc, char **argv);
void cmd_alias(int argc, char **argv);

void cmd_security(int argc, char **argv);
void cmd_users(int argc, char **argv);
void cmd_groups(int argc, char **argv);
void cmd_useradd(int argc, char **argv);
void cmd_groupadd(int argc, char **argv);
void cmd_aslr(int argc, char **argv);
void cmd_whoami(int argc, char **argv);
void cmd_id(int argc, char **argv);

#endif

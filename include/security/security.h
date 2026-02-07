/* Security Module
 * Memory protection, access control, and hardening features
 */

#ifndef _SECURITY_SECURITY_H
#define _SECURITY_SECURITY_H

#include <stdint.h>

#define CPU_FEATURE_PAE     (1 << 0)
#define CPU_FEATURE_NX      (1 << 1)
#define CPU_FEATURE_SMEP    (1 << 2)
#define CPU_FEATURE_SMAP    (1 << 3)

#define STACK_CHK_GUARD     0xDEADBEEF

#define ROOT_UID    0
#define ROOT_GID    0

#define MAX_USERS   64
#define MAX_GROUPS  64
#define MAX_USERNAME_LEN 32
#define MAX_GROUPNAME_LEN 32

typedef struct {
    uint32_t uid;
    uint32_t gid;
    char username[MAX_USERNAME_LEN];
    char password_hash[64];  
    char home_dir[64];
    char shell[32];
    int active;
} user_t;

typedef struct {
    uint32_t gid;
    char groupname[MAX_GROUPNAME_LEN];
    uint32_t members[MAX_USERS];  
    int member_count;
    int active;
} group_t;

#define CAP_CHOWN           (1ULL << 0)   /* Change file ownership */
#define CAP_DAC_OVERRIDE    (1ULL << 1)   /* Bypass file permissions */
#define CAP_KILL            (1ULL << 2)   /* Kill any process */
#define CAP_SETUID          (1ULL << 3)   /* Set UID */
#define CAP_SETGID          (1ULL << 4)   /* Set GID */
#define CAP_NET_BIND        (1ULL << 5)   /* Bind to privileged ports */
#define CAP_NET_RAW         (1ULL << 6)   /* Use raw sockets */
#define CAP_SYS_BOOT        (1ULL << 7)   /* Reboot/shutdown */
#define CAP_SYS_MODULE      (1ULL << 8)   /* Load kernel modules */
#define CAP_SYS_ADMIN       (1ULL << 9)   /* System administration */
#define CAP_SYS_PTRACE      (1ULL << 10)  /* Trace any process */
#define CAP_MKNOD           (1ULL << 11)  /* Create device nodes */

#define CAP_ALL             0xFFFFFFFFFFFFFFFFULL

#define SECCOMP_MODE_DISABLED   0
#define SECCOMP_MODE_STRICT     1   
#define SECCOMP_MODE_FILTER     2   

#define SECCOMP_RET_KILL    0x00000000  
#define SECCOMP_RET_TRAP    0x00030000  
#define SECCOMP_RET_ERRNO   0x00050000
#define SECCOMP_RET_ALLOW   0x7FFF0000  

typedef struct {
    uint32_t syscall_nr;
    uint32_t action;
} seccomp_rule_t;

#define MAX_SECCOMP_RULES 64
typedef struct {
    int mode;
    seccomp_rule_t rules[MAX_SECCOMP_RULES];
    int rule_count;
} seccomp_filter_t;

typedef struct {
    int enabled;
    uint32_t stack_entropy_bits;   
    uint32_t mmap_entropy_bits;    
    uint32_t heap_entropy_bits;    
} aslr_config_t;

/* Security initialization */
void security_init(void);

/* CPU feature detection */
uint32_t security_get_cpu_features(void);
int security_has_nx(void);
int security_has_smep(void);
int security_has_smap(void);

/* SMEP/SMAP control */
void security_enable_smep(void);
void security_enable_smap(void);
void security_disable_smap_temporarily(void);
void security_restore_smap(void);

/* Stack canary */
void security_init_stack_canary(void);
void __stack_chk_fail(void);

/* User management */
int user_add(const char *username, const char *password, uint32_t uid, uint32_t gid);
int user_remove(uint32_t uid);
user_t *user_get_by_uid(uint32_t uid);
user_t *user_get_by_name(const char *username);
int user_authenticate(const char *username, const char *password);
int user_set_password(uint32_t uid, const char *password);

/* Group management */
int group_add(const char *groupname, uint32_t gid);
int group_remove(uint32_t gid);
group_t *group_get_by_gid(uint32_t gid);
group_t *group_get_by_name(const char *groupname);
int group_add_member(uint32_t gid, uint32_t uid);
int group_remove_member(uint32_t gid, uint32_t uid);
int group_is_member(uint32_t gid, uint32_t uid);

/* Capability management */
uint64_t capability_get(uint32_t pid);
int capability_set(uint32_t pid, uint64_t caps);
int capability_has(uint32_t pid, uint64_t cap);
int capability_drop(uint32_t pid, uint64_t cap);

/* Seccomp */
int seccomp_set_mode(uint32_t pid, int mode);
int seccomp_add_rule(uint32_t pid, uint32_t syscall_nr, uint32_t action);
int seccomp_check(uint32_t pid, uint32_t syscall_nr);

/* ASLR */
void aslr_init(void);
int aslr_enable(void);
int aslr_disable(void);
uint32_t aslr_randomize_stack(uint32_t base);
uint32_t aslr_randomize_mmap(uint32_t base);
uint32_t aslr_randomize_heap(uint32_t base);

#endif

/* Security Module
 * Memory protection, access control, and hardening features
 */

#include <security/security.h>
#include <kernel/kernel.h>
#include <drivers/serial.h>
#include <fs/vfs.h>
#include <fs/ramfs.h>
#include <string.h>

static uint32_t cpu_features = 0;

static user_t users[MAX_USERS];
static group_t groups[MAX_GROUPS];

static aslr_config_t aslr_config = {
    .enabled = 1,
    .stack_entropy_bits = 8,
    .mmap_entropy_bits = 8,
    .heap_entropy_bits = 5
};

static uint32_t aslr_seed = 0x12345678;

uint32_t __stack_chk_guard = STACK_CHK_GUARD;


static void detect_cpu_features(void)
{
    uint32_t eax, ebx, ecx, edx;
    
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    
    if (edx & (1 << 6)) {
        cpu_features |= CPU_FEATURE_PAE;
    }
    
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000001));
    
    if (edx & (1 << 20)) {
        cpu_features |= CPU_FEATURE_NX;
    }
    
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(7), "c"(0));
    
    if (ebx & (1 << 7)) {
        cpu_features |= CPU_FEATURE_SMEP;
    }
    if (ebx & (1 << 20)) {
        cpu_features |= CPU_FEATURE_SMAP;
    }
}

uint32_t security_get_cpu_features(void)
{
    return cpu_features;
}

int security_has_nx(void)
{
    return (cpu_features & CPU_FEATURE_NX) != 0;
}

int security_has_smep(void)
{
    return (cpu_features & CPU_FEATURE_SMEP) != 0;
}

int security_has_smap(void)
{
    return (cpu_features & CPU_FEATURE_SMAP) != 0;
}


void security_enable_smep(void)
{
    if (!(cpu_features & CPU_FEATURE_SMEP)) return;
    
    uint32_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 20);  
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
    
    serial_puts("[SECURITY] SMEP enabled\n");
}

void security_enable_smap(void)
{
    if (!(cpu_features & CPU_FEATURE_SMAP)) return;
    
    uint32_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 21);  
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
    
    serial_puts("[SECURITY] SMAP enabled\n");
}

void security_disable_smap_temporarily(void)
{
    if (!(cpu_features & CPU_FEATURE_SMAP)) return;
    __asm__ volatile("stac" ::: "cc");  
}

void security_restore_smap(void)
{
    if (!(cpu_features & CPU_FEATURE_SMAP)) return;
    __asm__ volatile("clac" ::: "cc");  
}


void security_init_stack_canary(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    
    __stack_chk_guard = lo ^ hi ^ 0xDEADBEEF;
    
    if (__stack_chk_guard == 0) {
        __stack_chk_guard = 0xCAFEBABE;
    }
    
    serial_puts("[SECURITY] Stack canary initialized\n");
}

void __stack_chk_fail(void)
{
    serial_puts("[SECURITY] *** STACK SMASHING DETECTED ***\n");
    __asm__ volatile("cli; hlt");
    while(1);
}


static void simple_hash(const char *input, char *output)
{
    uint32_t hash = 5381;
    int c;
    
    while ((c = *input++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    for (int i = 0; i < 8; i++) {
        int nibble = (hash >> (28 - i * 4)) & 0xF;
        output[i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
    }
    output[8] = '\0';
}


int user_add(const char *username, const char *password, uint32_t uid, uint32_t gid)
{
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active && users[i].uid == uid) {
            return -1;  
        }
    }
    
    for (int i = 0; i < MAX_USERS; i++) {
        if (!users[i].active) {
            users[i].uid = uid;
            users[i].gid = gid;
            strncpy(users[i].username, username, MAX_USERNAME_LEN - 1);
            users[i].username[MAX_USERNAME_LEN - 1] = '\0';
            simple_hash(password, users[i].password_hash);
            strcpy(users[i].home_dir, "/home/");
            strcat(users[i].home_dir, username);
            strcpy(users[i].shell, "/bin/sh");
            users[i].active = 1;
            
            
            return 0;
        }
    }
    
    return -2;  
}

int user_remove(uint32_t uid)
{
    if (uid == ROOT_UID) return -1;  
    
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active && users[i].uid == uid) {
            memset(&users[i], 0, sizeof(user_t));
            return 0;
        }
    }
    return -1;
}

user_t *user_get_by_uid(uint32_t uid)
{
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active && users[i].uid == uid) {
            return &users[i];
        }
    }
    return NULL;
}

user_t *user_get_by_name(const char *username)
{
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active && strcmp(users[i].username, username) == 0) {
            return &users[i];
        }
    }
    return NULL;
}

int user_authenticate(const char *username, const char *password)
{
    user_t *user = user_get_by_name(username);
    if (!user) return -1;
    
    char hash[64];
    simple_hash(password, hash);
    
    if (strcmp(user->password_hash, hash) == 0) {
        return (int)user->uid;
    }
    return -1;
}

int user_set_password(uint32_t uid, const char *password)
{
    user_t *user = user_get_by_uid(uid);
    if (!user) return -1;
    
    simple_hash(password, user->password_hash);
    return 0;
}


int group_add(const char *groupname, uint32_t gid)
{
    for (int i = 0; i < MAX_GROUPS; i++) {
        if (groups[i].active && groups[i].gid == gid) {
            return -1;
        }
    }
    
    for (int i = 0; i < MAX_GROUPS; i++) {
        if (!groups[i].active) {
            groups[i].gid = gid;
            strncpy(groups[i].groupname, groupname, MAX_GROUPNAME_LEN - 1);
            groups[i].groupname[MAX_GROUPNAME_LEN - 1] = '\0';
            groups[i].member_count = 0;
            groups[i].active = 1;
            return 0;
        }
    }
    
    return -2;
}

int group_remove(uint32_t gid)
{
    if (gid == ROOT_GID) return -1;  
    
    for (int i = 0; i < MAX_GROUPS; i++) {
        if (groups[i].active && groups[i].gid == gid) {
            memset(&groups[i], 0, sizeof(group_t));
            return 0;
        }
    }
    return -1;
}

group_t *group_get_by_gid(uint32_t gid)
{
    for (int i = 0; i < MAX_GROUPS; i++) {
        if (groups[i].active && groups[i].gid == gid) {
            return &groups[i];
        }
    }
    return NULL;
}

group_t *group_get_by_name(const char *groupname)
{
    for (int i = 0; i < MAX_GROUPS; i++) {
        if (groups[i].active && strcmp(groups[i].groupname, groupname) == 0) {
            return &groups[i];
        }
    }
    return NULL;
}

int group_add_member(uint32_t gid, uint32_t uid)
{
    group_t *group = group_get_by_gid(gid);
    if (!group) return -1;
    
    for (int i = 0; i < group->member_count; i++) {
        if (group->members[i] == uid) return 0;
    }
    
    if (group->member_count >= MAX_USERS) return -2;
    
    group->members[group->member_count++] = uid;
    return 0;
}

int group_remove_member(uint32_t gid, uint32_t uid)
{
    group_t *group = group_get_by_gid(gid);
    if (!group) return -1;
    
    for (int i = 0; i < group->member_count; i++) {
        if (group->members[i] == uid) {
            for (int j = i; j < group->member_count - 1; j++) {
                group->members[j] = group->members[j + 1];
            }
            group->member_count--;
            return 0;
        }
    }
    return -1;
}

int group_is_member(uint32_t gid, uint32_t uid)
{
    group_t *group = group_get_by_gid(gid);
    if (!group) return 0;
    
    for (int i = 0; i < group->member_count; i++) {
        if (group->members[i] == uid) return 1;
    }
    return 0;
}


static uint32_t aslr_random(void)
{
    aslr_seed = aslr_seed * 1103515245 + 12345;
    return aslr_seed;
}

void aslr_init(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    aslr_seed = lo ^ hi;
    
    serial_puts("[SECURITY] ASLR initialized\n");
}

int aslr_enable(void)
{
    aslr_config.enabled = 1;
    serial_puts("[SECURITY] ASLR enabled\n");
    return 0;
}

int aslr_disable(void)
{
    aslr_config.enabled = 0;
    serial_puts("[SECURITY] ASLR disabled\n");
    return 0;
}

uint32_t aslr_randomize_stack(uint32_t base)
{
    if (!aslr_config.enabled) return base;
    
    uint32_t offset = (aslr_random() & ((1 << aslr_config.stack_entropy_bits) - 1)) << 12;
    return base - offset;  
}

uint32_t aslr_randomize_mmap(uint32_t base)
{
    if (!aslr_config.enabled) return base;
    
    uint32_t offset = (aslr_random() & ((1 << aslr_config.mmap_entropy_bits) - 1)) << 12;
    return base + offset;
}

uint32_t aslr_randomize_heap(uint32_t base)
{
    if (!aslr_config.enabled) return base;
    
    uint32_t offset = (aslr_random() & ((1 << aslr_config.heap_entropy_bits) - 1)) << 12;
    return base + offset;
}


void security_init(void)
{
    serial_puts("[SECURITY] Initializing security subsystem...\n");
    
    memset(users, 0, sizeof(users));
    memset(groups, 0, sizeof(groups));
    
    detect_cpu_features();
    
    if (cpu_features & CPU_FEATURE_PAE) {
        serial_puts("[SECURITY] PAE supported\n");
    }
    if (cpu_features & CPU_FEATURE_NX) {
        serial_puts("[SECURITY] NX bit supported\n");
    }
    if (cpu_features & CPU_FEATURE_SMEP) {
        serial_puts("[SECURITY] SMEP supported\n");
    }
    if (cpu_features & CPU_FEATURE_SMAP) {
        serial_puts("[SECURITY] SMAP supported\n");
    }
    
    security_init_stack_canary();
    
    aslr_init();
    
    serial_puts("[SECURITY] Creating users/groups\n");
    serial_puts("[SECURITY] Adding root group\n");
    group_add("root", ROOT_GID);
    serial_puts("[SECURITY] Adding root user\n");
    user_add("root", "root", ROOT_UID, ROOT_GID);
    serial_puts("[SECURITY] Adding root to group\n");
    group_add_member(ROOT_GID, ROOT_UID);
    
    group_add("wheel", 10);
    group_add("users", 100);
    
    serial_puts("[SECURITY] Checking SMEP/SMAP\n");
    if (cpu_features & CPU_FEATURE_SMEP) {
        serial_puts("[SECURITY] Enabling SMEP\n");
        security_enable_smep();
    }
    if (cpu_features & CPU_FEATURE_SMAP) {
        serial_puts("[SECURITY] Enabling SMAP\n");
        security_enable_smap();
    }
    
    serial_puts("[SECURITY] Security subsystem initialized\n");
}

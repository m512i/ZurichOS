#ifndef _KERNEL_SYMBOLS_H
#define _KERNEL_SYMBOLS_H

#include <stdint.h>

typedef struct {
    uint32_t addr;
    const char *name;
} ksym_t;

void symbols_init(void);
const char *symbols_lookup(uint32_t addr);
uint32_t symbols_lookup_name(const char *name);
void symbols_add(uint32_t addr, const char *name);

#endif

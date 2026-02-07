#ifndef _KERNEL_KERNEL_H
#define _KERNEL_KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ZURICHOS_VERSION_MAJOR  0
#define ZURICHOS_VERSION_MINOR  1
#define ZURICHOS_VERSION_PATCH  0
#define ZURICHOS_VERSION_STRING "0.1.0"

#define KERNEL_VMA              0xC0000000

#define UNUSED(x)               ((void)(x))
#define ARRAY_SIZE(arr)         (sizeof(arr) / sizeof((arr)[0]))
#define ALIGN_UP(x, align)      (((x) + ((align) - 1)) & ~((align) - 1))
#define ALIGN_DOWN(x, align)    ((x) & ~((align) - 1))

#define KB(x)                   ((x) * 1024UL)
#define MB(x)                   ((x) * 1024UL * 1024UL)
#define GB(x)                   ((x) * 1024UL * 1024UL * 1024UL)

#define PAGE_SIZE               4096
#define PAGE_SHIFT              12

#define PACKED                  __attribute__((packed))
#define ALIGNED(x)              __attribute__((aligned(x)))
#define NORETURN                __attribute__((noreturn))
#define UNUSED_ATTR             __attribute__((unused))
#define SECTION(x)              __attribute__((section(x)))

static inline void cli(void)
{
    __asm__ volatile("cli");
}

static inline void sti(void)
{
    __asm__ volatile("sti");
}

static inline void hlt(void)
{
    __asm__ volatile("hlt");
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val)
{
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val)
{
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline void io_wait(void)
{
    outb(0x80, 0);
}

void panic(const char *message) NORETURN;
void panic_with_regs(const char *message, uint32_t eip, uint32_t cs, 
                     uint32_t eflags, uint32_t err_code) NORETURN;

#endif

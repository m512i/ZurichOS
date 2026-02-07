#ifndef _KERNEL_ASSERT_H
#define _KERNEL_ASSERT_H

void __assert_fail(const char *expr, const char *file, int line, const char *func);

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            __assert_fail(#expr, __FILE__, __LINE__, __func__); \
        } \
    } while (0)

#define ASSERT_MSG(expr, msg) \
    do { \
        if (!(expr)) { \
            __assert_fail(msg, __FILE__, __LINE__, __func__); \
        } \
    } while (0)

#define ASSERT_NOT_REACHED() \
    __assert_fail("UNREACHABLE CODE", __FILE__, __LINE__, __func__)

#ifdef DEBUG
#define DEBUG_ASSERT(expr) ASSERT(expr)
#else
#define DEBUG_ASSERT(expr) ((void)0)
#endif

#endif

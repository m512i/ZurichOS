#ifndef _STDINT_H
#define _STDINT_H

/* exact width signed int types */
typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;

/* exact width unsigned int types */
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;

/* min width int types */
typedef int8_t              int_least8_t;
typedef int16_t             int_least16_t;
typedef int32_t             int_least32_t;
typedef int64_t             int_least64_t;

typedef uint8_t             uint_least8_t;
typedef uint16_t            uint_least16_t;
typedef uint32_t            uint_least32_t;
typedef uint64_t            uint_least64_t;

/* fastest min width int types */
typedef int32_t             int_fast8_t;
typedef int32_t             int_fast16_t;
typedef int32_t             int_fast32_t;
typedef int64_t             int_fast64_t;

typedef uint32_t            uint_fast8_t;
typedef uint32_t            uint_fast16_t;
typedef uint32_t            uint_fast32_t;
typedef uint64_t            uint_fast64_t;

/* int types capable of holding obj pointers */
typedef int32_t             intptr_t;
typedef uint32_t            uintptr_t;

/* greatest width int types */
typedef int64_t             intmax_t;
typedef uint64_t            uintmax_t;

/* limits of exact int types */
#define INT8_MIN            (-128)
#define INT16_MIN           (-32768)
#define INT32_MIN           (-2147483647 - 1)
#define INT64_MIN           (-9223372036854775807LL - 1)

#define INT8_MAX            (127)
#define INT16_MAX           (32767)
#define INT32_MAX           (2147483647)
#define INT64_MAX           (9223372036854775807LL)

#define UINT8_MAX           (255)
#define UINT16_MAX          (65535)
#define UINT32_MAX          (4294967295U)
#define UINT64_MAX          (18446744073709551615ULL)

/* limits of pointer int types */
#define INTPTR_MIN          INT32_MIN
#define INTPTR_MAX          INT32_MAX
#define UINTPTR_MAX         UINT32_MAX

/* macros for int constants */
#define INT8_C(c)           (c)
#define INT16_C(c)          (c)
#define INT32_C(c)          (c)
#define INT64_C(c)          (c ## LL)

#define UINT8_C(c)          (c)
#define UINT16_C(c)         (c)
#define UINT32_C(c)         (c ## U)
#define UINT64_C(c)         (c ## ULL)

#define INTMAX_C(c)         INT64_C(c)
#define UINTMAX_C(c)        UINT64_C(c)

#endif
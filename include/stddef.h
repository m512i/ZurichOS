#ifndef _STDDEF_H
#define _STDDEF_H

/* null ptr constant */
#define NULL ((void *)0)

/* size type */
typedef unsigned int size_t;

/* signed size type */
typedef int ssize_t;

/* ptr difference type */
typedef int ptrdiff_t;

/* wide character type */
typedef int wchar_t;

/* offset of member in structure */
#define offsetof(type, member) ((size_t)(&((type *)0)->member))

/* max alignment type */
typedef long double max_align_t;

#endif

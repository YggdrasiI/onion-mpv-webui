#pragma once

// for pipe2
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <unistd.h>


#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

#define __NEED_size_t

//#include <sys/types.h>

#define WRDE_DOOFFS  1
#define WRDE_APPEND  2
#define WRDE_NOCMD   4
#define WRDE_REUSE   8
#define WRDE_SHOWERR 16
#define WRDE_UNDEF   32
#include <sys/types.h>
typedef struct {
	size_t we_wordc;
	char **we_wordv;
	size_t we_offs;
} wordexp_t;

#define WRDE_NOSYS   -1
#define WRDE_NOSPACE 1
#define WRDE_BADCHAR 2
#define WRDE_BADVAL  3
#define WRDE_CMDSUB  4
#define WRDE_SYNTAX  5

int wordexp (const char *__restrict, wordexp_t *__restrict, int);
void wordfree (wordexp_t *);

#ifdef __cplusplus
}
#endif

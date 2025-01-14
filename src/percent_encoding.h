#pragma once

#include <stdlib.h>

#include <signal.h>
#include <string.h>
#include <stdio.h>

// Percent encoding functions…
char *encodeURI(const char *text);
char *encodeURIComponent(const char *text);
#define NON_STANDARD_FUNC
#ifdef NON_STANDARD_FUNC
// Just for testing a non-standard case of encoding
char *encodeNonCharNum(const char *text);
#endif
char *encode(const char *text);

/* …  and it's counterparts.
 *
 * Attention: 
 *   • Due the difference of the affected chares,
 *     decoding with the wrong function resolves
 *     too much %XX or too less %XX.
 *   • libonion uses decode() for all uri's (see codec.c) for decoding.
 *     Thus, if you want to decode uri components you has to
 *     double-encode them.
 *
 * decoding too much chars cannot be undone.
 *  encodeURI(decode(s)) != s
 */
char *decodeURI(const char *text);
char *decodeURIComponent(const char *text);
#ifdef NON_STANDARD_FUNC
char *decodeNonCharNum(const char *text);
#endif
char *decode(const char *text);

/* vim: set tabstop=4:softtabstop=4:shiftwidth=4:expandtab */
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

/********* Example usage ********/
/*
#include <stdio.h>
#include <locale.h>
#include "utf8.h"

int main(){
    char s[] = "Unicode\n2 bytes: ÄßͶۺ\n3 bytes: ẞṀ€‖\n4 bytes: 𝄞";
    printf("Input:\n%s\n", s);

#if 0 // With generation of unicode code point
    setlocale(LC_ALL, "C.UTF-8");
    int i = 0, j = 0;
    int d, l;
    unsigned int uc; // 4 byte unicode code point
    
    for(l=strlen(s); i<l; i=j)
    {
        uc=u8_nextchar(s,&j); d=j-i;
        // Note that shifting this into the head of the for loop 
        // Leads to unwanted read after string in u8_inc-call.

        printf("%lc", uc);  // just works after setlocale-call
        printf("%c%c%c ", 0xE2, 0x82, 0x80+d);
        // U+208X are supscript chars.
        //                              2    0    8    X
        // 2 byte unicode code point: 0010 0000 1000 xxxx
        // 3 byte Utf-8:  1110 0010
        //                10 000010
        //                10 00xxxx
    }
#else // Without generation of unicode code point
    int i = 0, j = 0;
    int d, l;
    for( l=strlen(s); i<l; i=j)
    {
        u8_inc(s,&j); d=j-i;
        // Note that shifting this into the head of the for loop 
        // Leads to unwanted read after string in u8_inc-call.

        if( d == 1 ) printf("%c₁ ", *(s+i));
        if( d == 2 ) printf("%c%c₂ ", *(s+i), *(s+i+1));
        if( d == 3 ) printf("%c%c%c₃ ", *(s+i), *(s+i+1), *(s+i+2));
        if( d == 4 ) printf("%c%c%c%c₄ ", *(s+i), *(s+i+1), *(s+i+2), *(s+i+3));
    }
#endif

    printf("\n");

    return 0;
    }
*/

/* is c the start of a utf8 sequence? */
#define isutf(c) (((c)&0xC0)!=0x80)

/* convert UTF-8 data to wide character */
int u8_toucs(uint32_t *dest, int sz, char *src, int srcsz);

/* the opposite conversion */
int u8_toutf8(char *dest, int sz, uint32_t *src, int srcsz);

/* single character to UTF-8 */
int u8_wc_toutf8(char *dest, uint32_t ch);

/* character number to byte offset */
int u8_offset(char *str, int charnum);

/* byte offset to character number */
int u8_charnum(char *s, int offset);

/* return next character, updating an index variable */
uint32_t u8_nextchar(char *s, int *i);

/* move to next character */
void u8_inc(char *s, int *i);

/* move to previous character */
void u8_dec(char *s, int *i);

/* returns length of next utf-8 sequence */
int u8_seqlen(char *s);

/* assuming src points to the character after a backslash, read an
   escape sequence, storing the result in dest and returning the number of
   input characters processed */
int u8_read_escape_sequence(char *src, uint32_t *dest);

/* given a wide character, convert it to an ASCII escape sequence stored in
   buf, where buf is "sz" bytes. returns the number of characters output. */
int u8_escape_wchar(char *buf, int sz, uint32_t ch);

/* convert a string "src" containing escape sequences to UTF-8 */
int u8_unescape(char *buf, int sz, char *src);

/* convert UTF-8 "src" to ASCII with escape sequences.
   if escape_quotes is nonzero, quote characters will be preceded by
   backslashes as well. */
int u8_escape(char *buf, int sz, char *src, int escape_quotes);

/* utility predicates used by the above */
int octal_digit(char c);
int hex_digit(char c);

/* return a pointer to the first occurrence of ch in s, or NULL if not
   found. character index of found character returned in *charn. */
char *u8_strchr(char *s, uint32_t ch, int *charn);

/* same as the above, but searches a buffer of a given size instead of
   a NUL-terminated string. */
char *u8_memchr(char *s, uint32_t ch, size_t sz, int *charn);

/* count the number of characters in a UTF-8 string */
int u8_strlen(char *s);

int u8_is_locale_utf8(char *locale);

/* printf where the format string and arguments may be in UTF-8.
   you can avoid this function and just use ordinary printf() if the current
   locale is UTF-8. */
int u8_vprintf(char *fmt, va_list ap);
int u8_printf(char *fmt, ...);

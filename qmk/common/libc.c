#include "libc.h"

size_t strlen(const char *str)
{
    const char *p = str;
    while(*(++p));

    return p - str;
}

void reverse(char *str)
{
    if(!str)
        return;


    char *end = str + strlen(str) - 1;

    while(str < end) {
        uint8_t c = *str;

        *str = *end;
        *end = c;

        str++;
        end--;
    }
}

// straight from musl libc
// https://git.musl-libc.org/cgit/musl/tree/src/ctype/isdigit.c
int isdigit(int c)
{
	return (unsigned)c-'0' < 10;
}

// http://git.musl-libc.org/cgit/musl/tree/src/stdlib/atoi.c
int atoi(const char *s)
{
    int n=0, neg=0;
    while (isspace(*s)) s++;
    switch (*s) {
        case '-': neg=1;
        case '+': s++;
    }
    /* Compute n as a negative number to avoid overflow on INT_MIN */
    while (isdigit(*s))
        n = 10*n - (*s++ - '0');
    return neg ? n : -n;
}

// http://git.musl-libc.org/cgit/musl/plain/src/ctype/isspace.c
int isspace(int _c)
{
	return _c == ' ' || (unsigned)_c-'\t' < 5;
}

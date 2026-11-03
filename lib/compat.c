#include "sklaff.h"

#ifdef LINUX
/* Simple strlcpy() */
size_t
strlcpy(char *dst, const char *src, size_t len)
{
    size_t n = len;
    char *d = dst;
    const char *sptr = src;

    if (n != 0) {
        while (--n != 0) {
            if ((*d++ = *sptr++) == '\0')
                break;
        }
    }
    if (n == 0) {
        if (len != 0) {
            *d = '\0';
            while (*sptr++) ;
        }
    }

    return sptr - src - 1;
}

/* Simple strlcat() */
size_t
strlcat(char *dst, const char *src, size_t len)
{
    char *d = dst;
    const char *s = src;
    size_t n = len;
    size_t dlen;

    /*
     * Find the end of dst, but never examine more than len bytes.
     */
    while (n != 0 && *d != '\0') {
        d++;
        n--;
    }

    dlen = d - dst;

    /*
     * dst was not NUL-terminated within len bytes.
     */
    if (n == 0) {
        while (*s++)
            ;
        return dlen + (s - src - 1);
    }

    /*
     * Append as much of src as fits, always leaving room for NUL.
     */
    while (*s != '\0') {
        if (n > 1) {
            *d++ = *s;
            n--;
        }
        s++;
    }

    *d = '\0';

    return dlen + (s - src);
}

#endif

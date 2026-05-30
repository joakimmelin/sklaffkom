#include <stdlib.h>
#include <string.h>

#include "sklaff.h"
#include "ext_globals.h"

/*
 * PL 2025-05-19
 * Convert SklaffKOM SF7 storage encoding to UTF-8.
 * Primarily for Usenet posts for now.
 * SF7 mapping used by SklaffKOM:
 *   } -> å
 *   { -> ä
 *   | -> ö
 *   ] -> Å
 *   [ -> Ä
 *   \ -> Ö
 *
 * Caller must free returned string.
 */

char *
sf7_to_utf8_dup(const char *src)
{
    const unsigned char *s;
    char *dst, *d;
    size_t len, outlen;

    if (src == NULL)
        return NULL;

    len = strlen(src);

    /*
     * Worst case: every byte becomes a 2-byte UTF-8 sequence.
     * +1 for NUL.
     */
    outlen = (len * 2) + 1;

    dst = malloc(outlen);
    if (dst == NULL)
        return NULL;

    s = (const unsigned char *)src;
    d = dst;

    while (*s) {
        switch (*s) {
        case '}': /* å U+00E5 */
            *d++ = (char)0xC3;
            *d++ = (char)0xA5;
            break;
        case '{': /* ä U+00E4 */
            *d++ = (char)0xC3;
            *d++ = (char)0xA4;
            break;
        case '|': /* ö U+00F6 */
            *d++ = (char)0xC3;
            *d++ = (char)0xB6;
            break;
        case ']': /* Å U+00C5 */
            *d++ = (char)0xC3;
            *d++ = (char)0x85;
            break;
        case '[': /* Ä U+00C4 */
            *d++ = (char)0xC3;
            *d++ = (char)0x84;
            break;
        case '\\': /* Ö U+00D6 */
            *d++ = (char)0xC3;
            *d++ = (char)0x96;
            break;
        default:
            *d++ = (char)*s;
            break;
        }
        s++;
    }

    *d = '\0';
    return dst;
}
/* PL 2026-05-26 
   For better encoding in mailtoss with modern e-mails
*/
char *
utf8_to_sf7_dup(const char *src)
{
    const unsigned char *s;
    char *out, *d;

    if (!src)
        return NULL;

    /*
     * UTF-8 -> SF7 blir aldrig längre för svenska tecken.
     */
    out = malloc(strlen(src) + 1);
    if (!out)
        return NULL;

    s = (const unsigned char *)src;
    d = out;

    while (*s) {
        if (s[0] == 0xC3 && s[1] != '\0') {
            switch (s[1]) {
            case 0xA5:  /* å */
                *d++ = '}';
                s += 2;
                continue;
            case 0xA4:  /* ä */
                *d++ = '{';
                s += 2;
                continue;
            case 0xB6:  /* ö */
                *d++ = '|';
                s += 2;
                continue;
            case 0x85:  /* Å */
                *d++ = ']';
                s += 2;
                continue;
            case 0x84:  /* Ä */
                *d++ = '[';
                s += 2;
                continue;
            case 0x96:  /* Ö */
                *d++ = '\\';
                s += 2;
                continue;
            default:
                break;
            }
        }

        *d++ = (char)*s++;
    }

    *d = '\0';
    return out;
}

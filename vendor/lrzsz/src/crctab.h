#ifndef LRZSZ_CRCTAB_H
#define LRZSZ_CRCTAB_H

extern const unsigned short crctab[256];
/* updcrc macro derived from article Copyright (C) 1986 Stephen Satchell. */
#define updcrc(cp, crc) ( crctab[((crc >> 8) & 255)] ^ (( (unsigned int)(crc&0x00ffffff)) << 8) ^ cp)
extern const unsigned long cr3tab[256];
#define UPDC32(b, c) (cr3tab[((int)c ^ b) & 0xff] ^ ((c >> 8) & 0x00FFFFFF))

#endif

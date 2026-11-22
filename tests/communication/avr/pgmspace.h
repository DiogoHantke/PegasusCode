#ifndef HOST_PGMSPACE_H
#define HOST_PGMSPACE_H
#include <string.h>
typedef const char *PGM_P;
#define PSTR(s) (s)
#define pgm_read_byte(p) (*(const unsigned char *)(p))
#define strcmp_P strcmp
#define strncmp_P strncmp
#endif

#ifndef PEGASUS_PROTOCOL_H
#define PEGASUS_PROTOCOL_H

#include <stdint.h>

#define FRAME_MAX 48

uint8_t protocol_crc(const char *text, uint8_t length);

/* Valida ASCII/CRC e separa sequencia e dados dentro do proprio frame. */
uint8_t protocol_decode(char *frame, uint8_t *seq, char **payload);

#endif

#ifndef PEGASUS_PROTOCOL_H
#define PEGASUS_PROTOCOL_H
#include <stdint.h>
#define FRAME_MAX 128
uint8_t protocol_crc(const char *text, uint8_t length);
/* Mutates frame, returns sequence and comma separated payload; strict ASCII/CRC. */
uint8_t protocol_decode(char *frame, uint8_t *seq, char **payload);
#endif

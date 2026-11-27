#ifndef PEGASUS_BUTTON_H
#define PEGASUS_BUTTON_H
#include <stdint.h>

enum { BUTTON_PRESS = 1, BUTTON_SHORT = 2, BUTTON_LONG = 4 };
void button_init(uint16_t now, uint8_t raw);
uint8_t button_update(uint16_t now, uint8_t raw);

#endif

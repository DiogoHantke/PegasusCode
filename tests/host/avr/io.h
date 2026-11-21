#ifndef TEST_AVR_IO_H
#define TEST_AVR_IO_H
#include <stdint.h>
extern uint8_t DDRD, DDRB, PORTD, PORTB, PIND;
enum { PD0=0, PD1, PD2, PD3, PD4, PD5, PD6, PD7 };
enum { PB0=0, PB1, PB2, PB3, PB4, PB5 };
#endif

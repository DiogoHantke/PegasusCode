#ifndef TEST_AVR_IO_H
#define TEST_AVR_IO_H
#include <stdint.h>
extern uint8_t DDRD, DDRB, PORTD, PORTB, PIND;
extern uint8_t ADMUX, ADCSRA, ADCH;
extern uint8_t TCCR0A, TCCR0B, TCNT0, OCR0A, OCR0B;
extern uint8_t UBRR0H, UBRR0L, UCSR0A, UCSR0B, UCSR0C;
uint8_t *test_udr(void);
#define UDR0 (*test_udr())
enum { PD0=0, PD1, PD2, PD3, PD4, PD5, PD6, PD7 };
enum { PB0=0, PB1, PB2, PB3, PB4, PB5 };
enum { REFS0=6, ADLAR=5, ADEN=7, ADSC=6, ADPS2=2, ADPS1=1, ADPS0=0 };
enum { DDD5=5, DDD6=6, WGM01=1, WGM00=0, COM0A1=7, COM0B1=5, CS00=0 };
enum { U2X0=1, UDRE0=5, TXEN0=3, UCSZ01=2, UCSZ00=1 };
#endif

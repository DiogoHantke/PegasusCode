#include <avr/io.h>
#include "usart_lib.h"

#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#if F_CPU != 16000000UL
#error "USART requer F_CPU=16000000UL"
#endif

#define USART_BAUD 115200UL
#define USART_UBRR ((F_CPU + 4UL * USART_BAUD) / (8UL * USART_BAUD) - 1UL)

void usart_init(void){

    UCSR0B = 0;
    DDRD &= ~(1 << PD0);
    PORTD |= (1 << PD1);
    DDRD |= (1 << PD1);

    UBRR0H = (uint8_t)(USART_UBRR >> 8);
    UBRR0L = (uint8_t)USART_UBRR;
    UCSR0A = (1 << U2X0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    UCSR0B = (1 << TXEN0);
}

void usart_transmit_char(char value){

    while (!(UCSR0A & (1 << UDRE0))){
    }
    UDR0 = (uint8_t)value;
}

void usart_transmit_string(const char *value){

    while (*value){
        usart_transmit_char(*value++);
    }
}

void usart_transmit_uint(uint16_t value){

    char digits[5];
    uint8_t count = 0;

    do {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value);

    while (count){
        usart_transmit_char(digits[--count]);
    }
}

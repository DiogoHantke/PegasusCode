#ifndef USART_LIB_H
#define USART_LIB_H

#include <stdint.h>

/* USART0: 115200 baud, 8N1, TX PD1. RX PD0 reservado, sem recepcao. */
void usart_init(void);
void usart_transmit_char(char value);
void usart_transmit_string(const char *value);
void usart_transmit_uint(uint16_t value);

#endif

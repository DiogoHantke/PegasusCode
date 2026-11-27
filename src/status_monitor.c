/* Diagnostico USB somente TX, sem comandos e sem espera pela UART. */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "status_monitor.h"
#include "adc_lib.h"
#include "calibration.h"
#include "timer_lib.h"

static char buffer[160];
static volatile uint8_t count, index_tx;
static uint16_t last;
static uint8_t first;
static const char *const names[] = {
    "INICIO", "CALIBRANDO", "PRONTO", "CORRIDA", "CHEGADA", "PARADO", "EMERGENCIA"
};

void status_monitor_init(void){

    UCSR0B = 0;
    UCSR0A = 1 << U2X0;
    UBRR0H = 0;
    UBRR0L = 16;
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    UCSR0B = 1 << TXEN0;
    count = index_tx = 0;
    first = 1;

}

void status_monitor_step(STATE state, uint8_t button){

    uint16_t now = timer1_elapsed_milliseconds();
    if(count || (!first && (uint16_t)(now - last) < 250U))return;
    first = 0;
    last = now;
    int length = snprintf(buffer, sizeof buffer,
        "BTN=%u ESTADO=%s FASE=%u FRONT=%02X ADC=%02X A6=%u A5=%u A4=%u A3=%u A2=%u A1=%u A7=%u A0=%u\r\n",
        (unsigned)button, names[state <= EMERGENCY ? state : EMERGENCY],
        (unsigned)calibration_phase(), (unsigned)calibration_front_mask(),
        (unsigned)adc_valid_mask(), (unsigned)adc_latest(6) * 4U,
        (unsigned)adc_latest(5) * 4U, (unsigned)adc_latest(4) * 4U,
        (unsigned)adc_latest(3) * 4U, (unsigned)adc_latest(2) * 4U,
        (unsigned)adc_latest(1) * 4U, (unsigned)adc_latest(7) * 4U,
        (unsigned)adc_latest(0) * 4U);
    if(length <= 0)return;
    if(length >= (int)sizeof buffer)length = sizeof buffer - 1;
    index_tx = 0;
    count = length;
    UCSR0B |= 1 << UDRIE0;

}

ISR(USART_UDRE_vect){
    UDR0 = buffer[index_tx++];
    if(index_tx >= count){
        UCSR0B &= ~(1 << UDRIE0);
        count = 0;
    }
}

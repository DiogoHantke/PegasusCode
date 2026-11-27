#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include "status_monitor.h"

uint8_t UCSR0A, UCSR0B, UCSR0C, UBRR0H, UBRR0L, UDR0;
static uint16_t now;
uint16_t timer1_elapsed_milliseconds(void){ return now; }
uint8_t calibration_phase(void){ return 2; }
uint8_t calibration_front_mask(void){ return 0x7E; }
uint8_t adc_valid_mask(void){ return 0xFF; }
uint8_t adc_latest(uint8_t channel){ return 100 + channel; }
void USART_UDRE_vect(void);
static void drain(char *output){
    unsigned count = 0;
    while(UCSR0B & (1 << UDRIE0)){
        assert(count < 159);
        USART_UDRE_vect();
        output[count++] = UDR0;
    }
    output[count] = 0;
}
int main(void){
    char output[160];
    status_monitor_init();
    assert(UCSR0B == (1 << TXEN0) && UBRR0L == 16);
    status_monitor_step(CALIBRATION, 1);
    assert(UCSR0B & (1 << UDRIE0));
    now = 500;
    status_monitor_step(WAITING, 0); /* Ocupado: nao sobrescreve o quadro. */
    drain(output);
    assert(strstr(output, "BTN=1 ESTADO=CALIBRANDO FASE=2 FRONT=7E ADC=FF"));
    assert(strstr(output, "A6=424") && strstr(output, "A0=400\r\n"));
    status_monitor_step(WAITING, 0);
    drain(output);
    assert(strstr(output, "BTN=0 ESTADO=PRONTO"));
    status_monitor_step(RUNNING, 0);
    assert(!(UCSR0B & (1 << UDRIE0)));
    now += 250;
    status_monitor_step(RUNNING, 0);
    drain(output);
    assert(strstr(output, "ESTADO=CORRIDA"));
    assert(UCSR0B == (1 << TXEN0));
    puts("USB TX: asynchronous frames, busy buffer and rate limit OK");
}

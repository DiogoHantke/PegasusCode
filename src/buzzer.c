/* Tone de 1 kHz em D10, sem Arduino e sem ocupar timers dos motores/relogio. */
#include <avr/io.h>
#include <avr/interrupt.h>
#include "buzzer.h"
#include "pegasus_config.h"
#include "timer_lib.h"

static uint8_t remaining, sounding;
static uint16_t changed;

static void silence(void){
    TIMSK2 &= ~(1 << OCIE2A);
    TCCR2B = 0;
    PORTB &= ~(1 << BUZZER);
}

static void tone_start(void){
    TCNT2 = 0;
    TIFR2 = 1 << OCF2A;
    TIMSK2 |= 1 << OCIE2A;
    TCCR2B = 1 << CS22; /* 16 MHz / 64 / 125 = 2000 interrupcoes/s. */
}

void buzzer_init(void){
    DDRB |= 1 << BUZZER;
    TCCR2A = 1 << WGM21;
    OCR2A = 124;
    remaining = sounding = 0;
    silence();
}

void buzzer_beep(uint8_t count){
    silence();
    remaining = count;
    sounding = !!count;
    changed = timer1_elapsed_milliseconds();
    if(count)tone_start();
}

void buzzer_update(void){
    if(!remaining)return;
    uint16_t now = timer1_elapsed_milliseconds();
    if((uint16_t)(now - changed) < 100U)return;
    changed = now;
    if(sounding){
        silence();
        sounding = 0;
        remaining--;
    }else{
        sounding = 1;
        tone_start();
    }
}

ISR(TIMER2_COMPA_vect){
    PORTB ^= 1 << BUZZER;
}

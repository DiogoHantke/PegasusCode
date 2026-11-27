#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <avr/io.h>
#include "buzzer.h"
#include "pegasus_config.h"

uint8_t DDRD, DDRB, PORTD, PORTB, PIND;
uint8_t TCCR2A, TCCR2B, TCNT2, OCR2A, TIMSK2, TIFR2;
static uint16_t now;
uint16_t timer1_elapsed_milliseconds(void){ return now; }
void TIMER2_COMPA_vect(void);
int main(void){
    PORTB = 1 << LED_L;
    buzzer_init();
    assert(DDRB & (1 << BUZZER));
    assert(!(PORTB & (1 << BUZZER)) && (PORTB & (1 << LED_L)));
    assert(TCCR2A == (1 << WGM21) && OCR2A == 124);
    assert(!TCCR2B && !(TIMSK2 & (1 << OCIE2A)));
    now = 65500;
    buzzer_beep(2);
    assert(TCCR2B == (1 << CS22) && (TIMSK2 & (1 << OCIE2A)));
    TIMER2_COMPA_vect();
    assert(PORTB & (1 << BUZZER));
    assert(PORTB & (1 << LED_L));
    TIMER2_COMPA_vect();
    assert(!(PORTB & (1 << BUZZER)));
    now += 99;
    buzzer_update();
    assert(TCCR2B);
    now++;
    buzzer_update();
    assert(!TCCR2B && !(TIMSK2 & (1 << OCIE2A)));
    now += 100;
    buzzer_update();
    assert(TCCR2B && (TIMSK2 & (1 << OCIE2A)));
    now += 100;
    buzzer_update();
    assert(!TCCR2B && !(TIMSK2 & (1 << OCIE2A)));
    now += 500;
    buzzer_update();
    assert(!TCCR2B);
    buzzer_beep(1);
    assert(TCCR2B);
    buzzer_beep(0);
    assert(!TCCR2B && !(PORTB & (1 << BUZZER)));
    puts("buzzer: OK");
}

#include <assert.h>
#include <stdio.h>
#include <avr/io.h>
#include "timer_lib.h"

uint8_t TCCR1A, TCCR1B, TCCR1C, TIMSK1, TIFR1;
uint16_t TCNT1, OCR1A;
void TIMER1_COMPA_vect(void);

int main(void){
    timer1_ctc_init();
    assert(OCR1A == 249 && TCNT1 == 0);
    assert(TCCR1B == (1 << WGM12));
    timer1_interrupt_enable();
    assert(TIMSK1 & (1 << OCIE1A));
    timer1_start();
    assert((TCCR1B & 7) == ((1 << CS11) | (1 << CS10)));

    for (uint32_t tick = 1; tick <= 65536UL; tick++){
        TIMER1_COMPA_vect();
        assert(timer1_elapsed_milliseconds() == (uint16_t)tick);
        assert(timer1_elapsed_seconds() == tick / 1000);
        uint8_t expected = TIMER_EVENT_1MS;
        if (!(tick % 10))expected |= TIMER_EVENT_10MS;
        if (!(tick % 100))expected |= TIMER_EVENT_100MS;
        if (!(tick % 1000))expected |= TIMER_EVENT_1S;
        assert(timer1_take_events() == expected);
        assert(!timer1_take_events());
    }
    timer1_stop();
    assert(!(TCCR1B & 7));
    assert(TCCR1B & (1 << WGM12));
    timer1_interrupt_disable();
    assert(!(TIMSK1 & (1 << OCIE1A)));
    timer1_ctc_init();
    assert(!timer1_elapsed_milliseconds() && !timer1_elapsed_seconds());
    puts("PASS Timer1: eventos, contadores, rollover, parada e reinicializacao");
    return 0;
}

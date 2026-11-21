#include "timer_lib.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#if F_CPU != 16000000UL
#error "Timer1 requer F_CPU=16000000UL"
#endif
#define TIMER1_TOP 249U

#define TIMER1_CLOCK_SELECT ((1U << CS11) | (1U << CS10))
#define TIMER1_CLOCK_MASK ((1U << CS12) | (1U << CS11) | (1U << CS10))

static volatile uint16_t timer1_count_1s;
static volatile uint16_t timer1_seconds;

void timer1_ctc_init(void) {

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {

        TCCR1B = 0;
        TCCR1A = 0;
        TCCR1C = 0;
        TIMSK1 &= ~(1U << OCIE1A);
        TCNT1 = 0;
        OCR1A = TIMER1_TOP;
        TIFR1 = (1U << OCF1A);
        TCCR1B = (1U << WGM12);
        timer1_count_1s = 0;
        timer1_seconds = 0;
    }

}
void timer1_start(void) {

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {

        TCCR1B = (TCCR1B & ~TIMER1_CLOCK_MASK) | TIMER1_CLOCK_SELECT;
    }

}
void timer1_stop(void) {

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {

        TCCR1B &= ~TIMER1_CLOCK_MASK;
    }

}
void timer1_interrupt_enable(void) {

    TIMSK1 |= (1U << OCIE1A);

}
uint16_t timer1_elapsed_seconds(void) {

    uint16_t seconds;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {

        seconds = timer1_seconds;
    }
    return seconds;

}
uint16_t timer1_elapsed_milliseconds(void) {

    uint16_t milliseconds;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {

        milliseconds = timer1_seconds * 1000U + timer1_count_1s;
    }
    return milliseconds;

}
ISR(TIMER1_COMPA_vect) {

    if (++timer1_count_1s >= 1000U) {

        timer1_count_1s = 0;
        ++timer1_seconds;
    }

}

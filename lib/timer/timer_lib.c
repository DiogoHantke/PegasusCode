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

static volatile uint8_t timer1_events;
static uint8_t timer1_count_10ms;
static uint8_t timer1_count_100ms;
static uint16_t timer1_count_1s;
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
        timer1_events = 0;
        timer1_count_10ms = 0;
        timer1_count_100ms = 0;
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
void timer1_interrupt_disable(void) {
    TIMSK1 &= ~(1U << OCIE1A);
}
uint16_t timer1_elapsed_seconds(void) {
    uint16_t seconds;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        seconds = timer1_seconds;
    }
    return seconds;
}
uint8_t timer1_take_events(void) {
    uint8_t events;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        events = timer1_events;
        timer1_events = 0;
    }
    return events;
}
ISR(TIMER1_COMPA_vect) {

    timer1_events |= TIMER_EVENT_1MS;

    if (++timer1_count_10ms >= 10U) {
        timer1_count_10ms = 0;
        timer1_events |= TIMER_EVENT_10MS;
    }
    if (++timer1_count_100ms >= 100U) {
        timer1_count_100ms = 0;
        timer1_events |= TIMER_EVENT_100MS;
    }
    if (++timer1_count_1s >= 1000U) {
        timer1_count_1s = 0;
        timer1_events |= TIMER_EVENT_1S;
        ++timer1_seconds;
    }
}

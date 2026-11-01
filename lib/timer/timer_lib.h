#ifndef TIMER_LIB_H
#define TIMER_LIB_H

#include <stdint.h>

#define TIMER_EVENT_1MS (1U << 0)
#define TIMER_EVENT_10MS (1U << 1)
#define TIMER_EVENT_100MS (1U << 2)
#define TIMER_EVENT_1S (1U << 3)

void timer1_ctc_init(void);
void timer1_start(void);
void timer1_stop(void);
void timer1_interrupt_enable(void);
void timer1_interrupt_disable(void);
uint8_t timer1_take_events(void);

#endif

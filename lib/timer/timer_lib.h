#ifndef TIMER_LIB_H
#define TIMER_LIB_H

#include <stdint.h>

void timer1_ctc_init(void);
void timer1_start(void);
void timer1_stop(void);
void timer1_interrupt_enable(void);
uint16_t timer1_elapsed_seconds(void);
uint16_t timer1_elapsed_milliseconds(void);

#endif

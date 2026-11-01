#ifndef PWM_LIB_H
#define PWM_LIB_H

#include <stdint.h>
#include <avr/io.h>

typedef enum {
    PWM_A,
    PWM_B
} pwm_channel_t;

void pwm_init(void);
void pwm_set_duty(pwm_channel_t channel, uint8_t duty);

#endif

#include "pwm_lib.h"
#include <avr/io.h>

#define PWM_MAX_DUTY 255U

void pwm_init(void)
{
    TCCR0A = 0x00;
    TCCR0B = 0x00;
    TCNT0  = 0x00;

    PORTD &= ~((1 << PD6) | (1 << PD5));
    DDRD |= (1 << DDD6) | (1 << DDD5);

    OCR0A = 0;
    OCR0B = 0;

    TCCR0A = (1 << WGM01)|(1 << WGM00)|(1 << COM0A1)|(1 << COM0B1);
    TCCR0B = (1 << CS00);
}

void pwm_set_duty(pwm_channel_t channel, uint8_t duty)
{

    if (channel == PWM_A){
        OCR0A = duty;

    }else{
        OCR0B = duty;

    }
}

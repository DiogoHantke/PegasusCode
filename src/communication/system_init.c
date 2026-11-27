#include <avr/io.h>
#include <avr/interrupt.h>

#include "pegasus_config.h"
#include "adc_lib.h"
#include "pwm_lib.h"
#include "timer_lib.h"
#include "pegasus_system.h"
#include "buzzer.h"
#include "status_monitor.h"

#ifdef PEGASUS_SUPERVISOR
#include "supervisor.h"
#endif

void init_sys(void){

    DDRD = (DDRD & ~(1 << BUTTON)) | (1 << MOTOR_R_IN1) | (1 << MOTOR_R_IN2) |
           (1 << MOTOR_L_PWM) | (1 << MOTOR_R_PWM);
    DDRB |= (1 << MOTOR_L_IN1) | (1 << MOTOR_L_IN2) | (1 << LED_L) | (1 << SENSOR_ON);
    PORTD &= ~(1 << BUTTON);
    PORTB &= ~((1 << LED_L) | (1 << SENSOR_ON));

    adc_init();
    pwm_init();
    timer1_ctc_init();
    timer1_interrupt_enable();
    timer1_start();
    buzzer_init();
    status_monitor_init();

#ifdef PEGASUS_SUPERVISOR
    supervisor_init();
#endif

    sei();

}

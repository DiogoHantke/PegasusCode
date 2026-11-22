#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include "pegasus_config.h"
#include "timer_lib.h"

static uint16_t emergency_time_init;
static uint8_t emergency_initialized;

void led_emergency_reset(void){

    if(emergency_initialized){

        emergency_initialized = 0;
        PORTB &= ~(1 << LED_L);

    }

}

void led_emergency_update(void){

    uint16_t now = timer1_elapsed_milliseconds();

    if(!emergency_initialized){

        emergency_time_init = now;
        emergency_initialized = 1;

    }

    uint16_t elapsed = (uint16_t)(now - emergency_time_init);

    if(elapsed >= 2000U){

        emergency_time_init = now;
        elapsed = 0;

    }

    if(elapsed < 100U || (elapsed >= 200U && elapsed < 300U)){
        PORTB |= (1 << LED_L);

    }else{
        PORTB &= ~(1 << LED_L);
    }

}

int main(void){

    /* Keep the motor driver inputs and sensor power low during the LED demo. */
    PORTB &= ~((1 << MOTOR_L_IN1) | (1 << MOTOR_L_IN2) | (1 << LED_L) | (1 << SENSOR_ON));
    PORTD &= ~((1 << MOTOR_R_IN1) | (1 << MOTOR_R_IN2) | (1 << MOTOR_L_PWM) | (1 << MOTOR_R_PWM));
    DDRB |= (1 << MOTOR_L_IN1) | (1 << MOTOR_L_IN2) | (1 << LED_L) | (1 << SENSOR_ON);
    DDRD |= (1 << MOTOR_R_IN1) | (1 << MOTOR_R_IN2) | (1 << MOTOR_L_PWM) | (1 << MOTOR_R_PWM);

    timer1_ctc_init();
    timer1_interrupt_enable();
    timer1_start();
    sei();

    for(;;){
        led_emergency_update();
    }
}

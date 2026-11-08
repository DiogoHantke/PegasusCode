#include <avr/io.h>
#include "pegasus_config.h"
#include "gpio_lib.h"

void gpio_init(void){
    DDRD = (DDRD & ~(1 << BUTTON)) | (1 << MOTOR_R_IN1) | (1 << MOTOR_R_IN2) | (1 << MOTOR_L_PWM) | (1 << MOTOR_R_PWM);
    DDRB |= (1 << MOTOR_L_IN1) | (1 << MOTOR_L_IN2) | (1 << LED_L) | (1 << SENSOR_ON);

    PORTD |= (1 << BUTTON);
    PORTB &= ~(1 << LED_L);
    PORTB &= ~(1 << SENSOR_ON);

}

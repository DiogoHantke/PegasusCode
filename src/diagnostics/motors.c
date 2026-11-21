#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include "pegasus_config.h"
#include "pwm_lib.h"
#include "adc_lib.h"
#include "timer_lib.h"

#define SENSOR_READ_DELAY_MS 1000U

int main(void){

    uint8_t value = 0;
    const uint8_t sensors[SENSORS_QUANTITY] = {QTR00_L, QTR01_F, QTR02_F, QTR03_F, QTR04_F, QTR05_F, QTR06_F, QTR07_L};
    uint8_t state = 0;
    uint8_t index = 1;
    uint8_t reverse_pending = 0;
    uint16_t stop_milliseconds = 0;
    uint16_t movement_start_milliseconds = 0;
    uint8_t read_pending = 0;

    PORTB &= ~((1 << MOTOR_L_IN1) | (1 << MOTOR_L_IN2) | (1 << SENSOR_ON));
    PORTD &= ~((1 << MOTOR_R_IN1) | (1 << MOTOR_R_IN2));

    DDRB |= (1 << MOTOR_L_IN1) | (1 << MOTOR_L_IN2) | (1 << SENSOR_ON);
    DDRD |= (1 << MOTOR_R_IN1) | (1 << MOTOR_R_IN2);

    pwm_init();
    adc_init();
    timer1_ctc_init();
    timer1_interrupt_enable();
    timer1_start();
    sei();

    PORTB |= (1 << SENSOR_ON);
    PORTB |= (1 << MOTOR_L_IN1);
    PORTD |= (1 << MOTOR_R_IN2);

    pwm_set_duty(PWM_A, 30);
    pwm_set_duty(PWM_B, 30);

    for(;;){

        if(reverse_pending){

            if((uint16_t)(timer1_elapsed_milliseconds() - stop_milliseconds) < MOTOR_REVERSE_DELAY_SECONDS * 1000U)continue;

            if(state){

                PORTB &= ~(1 << MOTOR_L_IN1);
                PORTB |= (1 << MOTOR_L_IN2);
                PORTD &= ~(1 << MOTOR_R_IN2);
                PORTD |= (1 << MOTOR_R_IN1);
            }else{

                PORTB &= ~(1 << MOTOR_L_IN2);
                PORTB |= (1 << MOTOR_L_IN1);
                PORTD &= ~(1 << MOTOR_R_IN1);
                PORTD |= (1 << MOTOR_R_IN2);
            }

            pwm_set_duty(PWM_A, 30);
            pwm_set_duty(PWM_B, 30);
            reverse_pending = 0;
            movement_start_milliseconds = timer1_elapsed_milliseconds();
            read_pending = 1;
        }

        if(read_pending){

            if((uint16_t)(timer1_elapsed_milliseconds() - movement_start_milliseconds) < SENSOR_READ_DELAY_MS)continue;
            read_pending = 0;
        }

        if(!adc_read(sensors[index], &value))continue;

        if(CAL_WHITE_HIGH ? value > CAL_LINE_THRESHOLD : value < CAL_LINE_THRESHOLD){

            pwm_set_duty(PWM_A, 0);
            pwm_set_duty(PWM_B, 0);

            state = !state;
            index = state ? 0 : 1;
            stop_milliseconds = timer1_elapsed_milliseconds();
            reverse_pending = 1;
            continue;
        }

        if(state){

            index = (index == QTR00_L) ? QTR07_L : QTR00_L;
        }else{

            if(++index >= SENSORS_QUANTITY-1)index = 1;
        }
    }

    return 0;

}

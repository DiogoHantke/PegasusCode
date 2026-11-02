#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <stdint.h>

#include "adc_lib.h"
#include "pwm_lib.h"
#include "timer_lib.h"

#define SENSORS_QUANTITY 8

#define BUTTON PD2

#define MOTOR_L_PWM   PD6
#define MOTOR_L_IN1   PD4
#define MOTOR_L_IN2   PD7

#define MOTOR_R_PWM   PD5
#define MOTOR_R_IN1   PB0
#define MOTOR_R_IN2   PB1

#define LED_L PB5

#define RX PD0
#define TX PD1

#define MOTORS_KEY 1 // If the MOTORS_KEY is 1, the motors will be started; otherwise, they will be stopped.

#define CAL_SAMPLES 10
#define CAL_TRANSITION_THRESHOLD 30
#define CAL_CONFIRM_TOLERANCE 10


#define QTR00 0x00
#define QTR01 0x01
#define QTR02 0x02
#define QTR03 0x03
#define QTR04 0x04
#define QTR05 0x05
#define QTR06 0x06
#define QTR07 0x07

typedef enum {
    CALIBRATION,
    WAITING,
    RUNNING,
    FINISH,
    STOP
} STATE;

typedef enum {
    INIT_CALIBRATION,
    FRONT_CALIBRATION,
    LATERAL_CALIBRATION,
    DIAGNOSTIC,
    CALIBRATION_FAULT
} STATE_CALIBRATION;

void calibration_sensor(STATE *state);

int main(void){

    STATE state = CALIBRATION;

    DDRD = (DDRD & ~(1 << BUTTON)) | (1 << MOTOR_L_IN1) | (1 << MOTOR_L_IN2) | (1 << MOTOR_L_PWM) | (1 << MOTOR_R_PWM);
    DDRB |= (1 << MOTOR_R_IN1) | (1 << MOTOR_R_IN2) | (1 << LED_L);

    PORTD &= ~(1 << BUTTON);
    PORTB |= (1 << LED_L);

    adc_init();
    pwm_init();
    timer1_ctc_init();

    for (;;){
        switch (state)
        {
        case CALIBRATION:
            calibration_sensor(&state);
            break;

        case WAITING:
            break;

        case RUNNING:
            break;

        case FINISH:
            break;

        case STOP:
            break;

        default:
            break;
        }
    }

    return 0;
}

void calibration_sensor(STATE *state) {

    static uint8_t sensors_min[SENSORS_QUANTITY];
    static uint8_t sensors_max[SENSORS_QUANTITY];

    static const uint8_t sensors_indentifier[SENSORS_QUANTITY] = {QTR00, QTR01, QTR02, QTR03, QTR04, QTR05, QTR06, QTR07};

    static uint8_t value_adc = 0, index = 0, surface_black_value = 0;
    static uint16_t value_mean[SENSORS_QUANTITY] = {0};
    static uint8_t count_mean[SENSORS_QUANTITY] = {0};

    static uint8_t previous_mean[SENSORS_QUANTITY] = {0};
    static uint8_t candidate_mean[SENSORS_QUANTITY] = {0};

    static uint8_t mean_initialized[SENSORS_QUANTITY] = {0};
    static uint8_t transition_pending[SENSORS_QUANTITY] = {0};

    static STATE_CALIBRATION state_calibration = INIT_CALIBRATION;

    uint8_t button_state = PIND & (1 << BUTTON);

    // starting timer. If the time value is greater than the threshold, break the loop, and blink the arduino led, in the pattern of the sensor number with possible defect.
    // if not, leave the led on.

    switch (state_calibration)
    {
    case INIT_CALIBRATION:

        if (button_state){

            for (uint8_t index = 0; index < SENSORS_QUANTITY; index++) {
                sensors_min[index] = 255;
                sensors_max[index] = 0;

                value_mean[index] = 0;
                count_mean[index] = 0;

                previous_mean[index] = 0;
                candidate_mean[index] = 0;

                mean_initialized[index] = 0;
                transition_pending[index] = 0;
            }

            index = 0;

            if (!adc_read(sensors_indentifier[index], &surface_black_value)) break;

            if(MOTORS_KEY){
                PORTD &= ~(1 << MOTOR_L_IN2);
                PORTD |=  (1 << MOTOR_L_IN1);

                PORTB &= ~(1 << MOTOR_R_IN2);
                PORTB |=  (1 << MOTOR_R_IN1);

                pwm_set_duty(PWM_A, 20);
                pwm_set_duty(PWM_B, 20);
            }

            PORTB &= ~(1 << LED_L);

            timer1_start();

            state_calibration = FRONT_CALIBRATION;
        }
        break;

    case FRONT_CALIBRATION:

        if (adc_read(sensors_indentifier[index], &value_adc)){

            value_mean[index] += value_adc;

            if (++count_mean[index] >= CAL_SAMPLES){ // reduces noise and surface irregularities

                uint8_t absolute_mean = (uint8_t)(value_mean[index] / count_mean[index]); // obtem o valor da média em uma janela de 10 amostras.
                uint8_t difference = 0, accepted_mean = 0;

                if (!mean_initialized[index]){

                    previous_mean[index] = absolute_mean;
                    mean_initialized[index] = 1;
                    accepted_mean = 1;

                }else if (transition_pending[index]){

                    difference = (absolute_mean > candidate_mean[index]) ?
                        (absolute_mean - candidate_mean[index]) :
                        (candidate_mean[index] - absolute_mean);

                    if (difference <= CAL_CONFIRM_TOLERANCE){

                        previous_mean[index] = absolute_mean;
                        transition_pending[index] = 0;
                        accepted_mean = 1;

                    }else{

                        difference = (absolute_mean > previous_mean[index]) ?
                            (absolute_mean - previous_mean[index]) :
                            (previous_mean[index] - absolute_mean);

                        if (difference >= CAL_TRANSITION_THRESHOLD){

                            candidate_mean[index] = absolute_mean;

                        }else{

                            transition_pending[index] = 0;
                            previous_mean[index] = absolute_mean;
                            accepted_mean = 1;
                        }
                    }

                }else{

                    difference = (absolute_mean > previous_mean[index]) ?
                        (absolute_mean - previous_mean[index]) :
                        (previous_mean[index] - absolute_mean);

                    if (difference >= CAL_TRANSITION_THRESHOLD){

                        candidate_mean[index] = absolute_mean;
                        transition_pending[index] = 1;

                    }else{

                        previous_mean[index] = absolute_mean;
                        accepted_mean = 1;
                    }
                }

                if (accepted_mean){

                    if (sensors_max[index] < absolute_mean){
                        sensors_max[index] = absolute_mean;
                    }

                    if (sensors_min[index] > absolute_mean){
                        sensors_min[index] = absolute_mean;
                    }
                }

                count_mean[index] = 0;
                value_mean[index] = 0;
            }

            if (++index >= SENSORS_QUANTITY-1) index = 1; // tira os sensores laterais da conferencia frontal
        }

        break;

    case DIAGNOSTIC:
        break;

    case CALIBRATION_FAULT:
        break;

    default:
        break;
    }
}
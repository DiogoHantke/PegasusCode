#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include "adc_lib.h"
#include "pwm_lib.h"
#include "timer_lib.h"

#define SENSORS_QUANTITY 8

#define BUTTON PD2

#define MOTOR_L_PWM   PD5
#define MOTOR_L_IN1   PB0
#define MOTOR_L_IN2   PB1

#define MOTOR_R_PWM   PD6
#define MOTOR_R_IN1   PD4
#define MOTOR_R_IN2   PD7

#define LED_L PB5
#define SENSOR_ON PB3

#define RX PD0
#define TX PD1

#define MOTORS_KEY 1

#define CAL_SAMPLES 10
#define CAL_CONFIRM_TOLERANCE 10
#define CAL_MIN_CONTRAST 40
#define CAL_TIMEOUT_SECONDS 5U
#define THRESHOLD_LINE 50
#define PERCENT_MOTOR_CALIBRATE 20
#define MOTOR_REVERSE_DELAY_SECONDS 2U
#define BUTTON_TIMEOUT_SECONDS 2U

#define QTR00_L 0x00
#define QTR01_F 0x01
#define QTR02_F 0x02
#define QTR03_F 0x03
#define QTR04_F 0x04
#define QTR05_F 0x05
#define QTR06_F 0x06
#define QTR07_L 0x07

typedef enum {

    INIT,
    CALIBRATION,
    WAITING,
    RUNNING,
    FINISH,
    STOP,
    EMERGENCY

} STATE;

typedef enum {

    INIT_CALIBRATION,
    INIT_FRONT_CALIBRATION,
    FRONT_CALIBRATION,
    INIT_LATERAL_CALIBRATION,
    LATERAL_CALIBRATION,
    DIAGNOSTIC,
    CALIBRATION_FAULT

} STATE_CALIBRATION;

typedef struct {

    uint8_t channel;

    uint8_t min;
    uint8_t max;

    uint16_t value_mean;
    uint8_t count_mean;

    uint8_t candidate_mean;

    uint8_t mean_initialized;
    uint8_t transition_pending;

    uint8_t contrast;
    uint8_t aproved;

} SENSOR;

void calibration_sensor(STATE *state);
void sensor_calibration_update(SENSOR *sensor, uint8_t value_adc);

/*============================ FUNÇÃO PRINCIPAL ============================*/

int main(void){

    STATE state = INIT;

    DDRD = (DDRD & ~(1 << BUTTON)) | (1 << MOTOR_R_IN1) | (1 << MOTOR_R_IN2) | (1 << MOTOR_L_PWM) | (1 << MOTOR_R_PWM);
    DDRB |= (1 << MOTOR_L_IN1) | (1 << MOTOR_L_IN2) | (1 << LED_L) | (1 << SENSOR_ON);

    PORTD &= ~(1 << BUTTON);
    PORTB |= (1 << LED_L);
    PORTB &= ~(1 << SENSOR_ON);

    adc_init();
    pwm_init();
    timer1_ctc_init();
    timer1_interrupt_enable();
    sei();

    pwm_set_duty(PWM_A, 0);
    pwm_set_duty(PWM_B, 0);

    for (;;){
        switch (state)
        {
        case INIT:

            timer1_start();

            uint32_t button_start_seconds = timer1_elapsed_seconds();

            while (!(PIND & (1 << BUTTON))){
                if((uint16_t)(timer1_elapsed_seconds() - button_start_seconds) >= BUTTON_TIMEOUT_SECONDS){

                    state = WAITING;
                    timer1_stop();
                    break;
                }
            }

            if(state == WAITING)break;

            state = CALIBRATION;
            
            break;

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

        case EMERGENCY:
            break;

        default:
            break;
        }
    }

    return 0;
}

/*============================ FUNÇÕES AUXILIARES ============================*/


void calibration_sensor(STATE *state){

    static SENSOR sensors[SENSORS_QUANTITY] = {
        {.channel = QTR00_L},
        {.channel = QTR01_F},
        {.channel = QTR02_F},
        {.channel = QTR03_F},
        {.channel = QTR04_F},
        {.channel = QTR05_F},
        {.channel = QTR06_F},
        {.channel = QTR07_L}
    };

    static uint8_t index = 1;
    static uint8_t lateral_initialized = 0;

    static uint16_t calibration_start_seconds = 0;
    static uint16_t lateral_stop_seconds = 0;

    static STATE_CALIBRATION state_calibration = INIT_CALIBRATION;

    uint8_t value_adc = 0;

    switch (state_calibration){

    case INIT_CALIBRATION: {

        for(uint8_t index_reset = 0; index_reset < SENSORS_QUANTITY; index_reset++){

            sensors[index_reset].min = 255;
            sensors[index_reset].max = 0;

            sensors[index_reset].value_mean = 0;
            sensors[index_reset].count_mean = 0;

            sensors[index_reset].candidate_mean = 0;

            sensors[index_reset].mean_initialized = 0;
            sensors[index_reset].transition_pending = 0;

            sensors[index_reset].contrast = 0;
            sensors[index_reset].aproved = 0;
        }

        pwm_set_duty(PWM_A, 0);
        pwm_set_duty(PWM_B, 0);

        PORTB |= (1 << SENSOR_ON);
        PORTB &= ~(1 << LED_L);

        timer1_start();

        state_calibration = INIT_FRONT_CALIBRATION;

        break;
    }

    case INIT_FRONT_CALIBRATION: {

        index = 1;

        pwm_set_duty(PWM_A, 0);
        pwm_set_duty(PWM_B, 0);

        PORTB &= ~(1 << MOTOR_L_IN2);
        PORTB |= (1 << MOTOR_L_IN1);

        PORTD &= ~(1 << MOTOR_R_IN2);
        PORTD |= (1 << MOTOR_R_IN1);

        if(MOTORS_KEY){
            pwm_set_duty(PWM_B, PERCENT_MOTOR_CALIBRATE);
            pwm_set_duty(PWM_A, PERCENT_MOTOR_CALIBRATE);
        }

        calibration_start_seconds = timer1_elapsed_seconds();

        state_calibration = FRONT_CALIBRATION;

        break;
    }

    case FRONT_CALIBRATION: {

        if((uint16_t)(timer1_elapsed_seconds() - calibration_start_seconds) >= CAL_TIMEOUT_SECONDS){

            pwm_set_duty(PWM_A, 0);
            pwm_set_duty(PWM_B, 0);

            state_calibration = INIT_LATERAL_CALIBRATION;

            break;
        }

        SENSOR *sensor = &sensors[index];

        if(sensor->aproved){

            if(++index >= SENSORS_QUANTITY-1){
                index = 1;
            }

            break;
        }

        if(adc_read(sensor->channel, &value_adc)){

            sensor_calibration_update(sensor, value_adc);

            if(++index >= SENSORS_QUANTITY-1){
                index = 1;
            }
        }

        uint8_t count_aproved = 0;

        for(uint8_t index_conference = 1; index_conference < SENSORS_QUANTITY-1; index_conference++){

            SENSOR *sensor_conference = &sensors[index_conference];

            if(!sensor_conference->aproved && sensor_conference->min <= sensor_conference->max){

                sensor_conference->contrast = sensor_conference->max - sensor_conference->min;

                if(sensor_conference->contrast >= CAL_MIN_CONTRAST){
                    sensor_conference->aproved = 1;
                }
            }

            if(sensor_conference->aproved)count_aproved++;
        }

        if(count_aproved >= SENSORS_QUANTITY-2){

            pwm_set_duty(PWM_A, 0);
            pwm_set_duty(PWM_B, 0);

            state_calibration = INIT_LATERAL_CALIBRATION;
        }

        break;
    }

    case INIT_LATERAL_CALIBRATION: {

        if(!lateral_initialized){

            pwm_set_duty(PWM_A, 0);
            pwm_set_duty(PWM_B, 0);

            lateral_stop_seconds = timer1_elapsed_seconds();
            lateral_initialized = 1;

            break;
        }

        if((uint16_t)(timer1_elapsed_seconds() - lateral_stop_seconds) < MOTOR_REVERSE_DELAY_SECONDS){
            break;
        }

        PORTB &= ~(1 << MOTOR_L_IN1);
        PORTB |= (1 << MOTOR_L_IN2);

        PORTD &= ~(1 << MOTOR_R_IN1);
        PORTD |= (1 << MOTOR_R_IN2);

        index = 0;

        if(MOTORS_KEY){
            pwm_set_duty(PWM_B, PERCENT_MOTOR_CALIBRATE);
            pwm_set_duty(PWM_A, PERCENT_MOTOR_CALIBRATE);
        }

        calibration_start_seconds = timer1_elapsed_seconds();

        lateral_initialized = 0;

        state_calibration = LATERAL_CALIBRATION;

        break;
    }

    case LATERAL_CALIBRATION: {

        SENSOR *sensor = &sensors[index];

        if(sensors[0].aproved && sensors[SENSORS_QUANTITY-1].aproved){
            pwm_set_duty(PWM_A, 0);
            pwm_set_duty(PWM_B, 0);

            state_calibration = DIAGNOSTIC;
            break;
        }

        if((uint16_t)(timer1_elapsed_seconds() - calibration_start_seconds) >= CAL_TIMEOUT_SECONDS){

            pwm_set_duty(PWM_B, 0);
            pwm_set_duty(PWM_A, 0);

            state_calibration = DIAGNOSTIC;

            break;
        }

        if(sensor->aproved){
            index = (index == 0) ? 7 : 0;
            break;
        }

        if(adc_read(sensor->channel, &value_adc)){

            sensor_calibration_update(sensor, value_adc);

            if(!sensor->aproved && sensor->min <= sensor->max){

                sensor->contrast = sensor->max - sensor->min;

                if(sensor->contrast >= CAL_MIN_CONTRAST){
                    sensor->aproved = 1;
                }
            }

            index = (index == 0) ? 7 : 0;
        }

        break;
    }

    case DIAGNOSTIC: {

        pwm_set_duty(PWM_A, 0);
        pwm_set_duty(PWM_B, 0);

        break;
    }

    case CALIBRATION_FAULT: {

        pwm_set_duty(PWM_A, 0);
        pwm_set_duty(PWM_B, 0);

        PORTB &= ~(1 << SENSOR_ON);

        *state = EMERGENCY;

        break;
    }

    default:
        break;
    }
}

void sensor_calibration_update(SENSOR *sensor, uint8_t value_adc){

    uint8_t absolute_mean = 0;
    uint8_t difference = 0;

    sensor->value_mean += value_adc;

    if (++sensor->count_mean >= CAL_SAMPLES){

        absolute_mean = (uint8_t)(sensor->value_mean / sensor->count_mean);

        sensor->count_mean = 0;
        sensor->value_mean = 0;

        if(!sensor->mean_initialized){

            sensor->mean_initialized = 1;
            sensor->candidate_mean = absolute_mean;

        }else{

            difference = (sensor->candidate_mean > absolute_mean) ? (sensor->candidate_mean - absolute_mean) : (absolute_mean - sensor->candidate_mean);

            if(difference < THRESHOLD_LINE && !sensor->transition_pending){

                if(sensor->max < sensor->candidate_mean)sensor->max = sensor->candidate_mean;
                if(sensor->min > sensor->candidate_mean)sensor->min = sensor->candidate_mean;

            }else if(difference >= THRESHOLD_LINE && !sensor->transition_pending){

                sensor->transition_pending = 1;

            }else if(sensor->transition_pending){

                if(difference < CAL_CONFIRM_TOLERANCE){

                    if(sensor->max < sensor->candidate_mean)sensor->max = sensor->candidate_mean;
                    if(sensor->min > sensor->candidate_mean)sensor->min = sensor->candidate_mean;

                    sensor->transition_pending = 0;
                }
            }

            sensor->candidate_mean = absolute_mean;
        }
    }
}

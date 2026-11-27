/* Varredura automatica limitada por tempo, sem exigir oito sensores perfeitos. */
#include <avr/io.h>
#include <string.h>
#include "calibration_internal.h"
#include "adc_lib.h"
#include "pwm_lib.h"
#include "timer_lib.h"

static SENSOR sensors[SENSORS_QUANTITY];
static uint8_t phase, index_sensor, front_mask;
static uint16_t changed;

static void stop_motors(void){
    pwm_set_raw(PWM_A, 0);
    pwm_set_raw(PWM_B, 0);
}

static void drive(uint8_t forward){

    stop_motors();
    PORTB &= ~((1 << MOTOR_L_IN1) | (1 << MOTOR_L_IN2));
    PORTD &= ~((1 << MOTOR_R_IN1) | (1 << MOTOR_R_IN2));
    PORTB |= 1 << (forward ? MOTOR_L_FORWARD : MOTOR_L_REVERSE);
    PORTD |= 1 << (forward ? MOTOR_R_FORWARD : MOTOR_R_REVERSE);
    pwm_set_raw(PWM_A, CAL_PWM);
    pwm_set_raw(PWM_B, CAL_PWM);

}

void calibration_reset(void){

    stop_motors();
    memset(sensors, 0, sizeof sensors);
    for(uint8_t channel = 0; channel < SENSORS_QUANTITY; channel++){
        sensors[channel].channel = channel;
        sensors[channel].min = 255;
    }
    index_sensor = front_mask = phase = 0;

}

void calibration_sensor(STATE *state){

    if(!state || *state != CALIBRATION)return;
    uint16_t now = timer1_elapsed_milliseconds();
    if(phase == 0){
        PORTB |= 1 << SENSOR_ON;
        changed = now;
        phase = 1;
        drive(1);
    }
    uint8_t raw;
    if(adc_read(index_sensor, &raw)){
        sensor_calibration_update(&sensors[index_sensor], raw);
        if(++index_sensor >= SENSORS_QUANTITY)index_sensor = 0;
    }
    uint16_t elapsed = (uint16_t)(now - changed);
    if(phase == 1 && elapsed >= CAL_LEG_MS){
        stop_motors();
        changed = now;
        phase = 2;
    }else if(phase == 2 && elapsed >= CAL_REVERSE_PAUSE_MS){
        drive(0);
        changed = now;
        phase = 3;
    }else if(phase == 3 && elapsed >= CAL_LEG_MS){
        stop_motors();
        front_mask = 0;
        for(uint8_t channel = 0; channel < SENSORS_QUANTITY; channel++){
            uint8_t valid = diagnostic_sensor_approved(&sensors[channel]);
            if(channel >= QTR01_F && channel <= QTR06_F && valid)
                front_mask |= 1U << channel;
        }
        PORTB &= ~(1 << SENSOR_ON);
        phase = 4;
        /* Laterais e canais sem contraste nao bloqueiam os frontais restantes. */
        *state = front_mask ? WAITING : EMERGENCY;
    }

}

void calibration_get(uint8_t channel, uint8_t *low, uint8_t *high, uint8_t *approved){

    if(channel >= SENSORS_QUANTITY || !low || !high || !approved)return;
    *low = sensors[channel].min;
    *high = sensors[channel].max;
    *approved = sensors[channel].approved;

}

uint8_t calibration_phase(void){ return phase; }
uint8_t calibration_front_mask(void){ return front_mask; }

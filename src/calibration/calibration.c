/* Mantém o contexto privado e encaminha cada estado para sua etapa. */

#include <avr/io.h>

#include "calibration_internal.h"
#include "pwm_lib.h"
#include "timer_lib.h"

static CALIBRATION_CONTEXT calibration = {

    .sensors = {

        {.channel = QTR00_L},
        {.channel = QTR01_F},
        {.channel = QTR02_F},
        {.channel = QTR03_F},
        {.channel = QTR04_F},
        {.channel = QTR05_F},
        {.channel = QTR06_F},
        {.channel = QTR07_L}
    },
    .index = 1,
    .state_calibration = INIT_CALIBRATION,
    .state_diagnostic = DIAGNOSTIC_CHECK

};

void calibration_reset(void){

    calibration.state_calibration = INIT_CALIBRATION;

}

static void calibration_fault_step(STATE *state){

    pwm_set_duty(PWM_A, 0);
    pwm_set_duty(PWM_B, 0);

    PORTB &= ~(1 << SENSOR_ON);

    timer1_start();

    *state = EMERGENCY;

}

void calibration_sensor(STATE *state){

    if(!state)return;

    if(*state == EMERGENCY)calibration.state_calibration = CALIBRATION_FAULT;

    switch(calibration.state_calibration){

    case INIT_CALIBRATION:

        calibration_prepare(&calibration);
        break;

    case INIT_FRONT_CALIBRATION:

        calibration_front_start(&calibration);
        break;

    case FRONT_CALIBRATION:

        calibration_front_step(&calibration);
        break;

    case INIT_LATERAL_CALIBRATION:

        calibration_lateral_start(&calibration);
        break;

    case LATERAL_CALIBRATION:

        calibration_lateral_step(&calibration);
        break;

    case DIAGNOSTIC:

        calibration_diagnostic_step(&calibration, state);
        break;

    case CALIBRATION_FAULT:
        calibration_fault_step(state);
        break;

    default:

        break;
    }

}

void calibration_get(uint8_t channel, uint8_t *low, uint8_t *high, uint8_t *approved){

    if(channel >= SENSORS_QUANTITY)return;

    *low = calibration.sensors[channel].min;
    *high = calibration.sensors[channel].max;
    *approved = calibration.sensors[channel].approved;

}

uint8_t calibration_phase(void){

    return calibration.state_calibration;

}

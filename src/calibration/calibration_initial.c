/* Preparação e aquisição inicial: frontais, depois laterais. */

#include <avr/io.h>

#include "calibration_internal.h"
#include "adc_lib.h"
#include "pwm_lib.h"
#include "timer_lib.h"

void calibration_prepare(CALIBRATION_CONTEXT *context){

    context->recalibration_initialized = 0;
    context->recalibration_count = 0;
    context->recalibration_mask = 0;
    context->state_diagnostic = DIAGNOSTIC_CHECK;
    context->white_seen = 0;
    context->initial_crossed_line = 0;

    for(uint8_t index_reset = 0; index_reset < SENSORS_QUANTITY; index_reset++){

        context->reference_white_count[index_reset] = 0;
        context->reference_black_count[index_reset] = 0;

        context->sensors[index_reset].min = 255;
        context->sensors[index_reset].max = 0;

        context->sensors[index_reset].value_mean = 0;
        context->sensors[index_reset].count_mean = 0;

        context->sensors[index_reset].candidate_mean = 0;

        context->sensors[index_reset].mean_initialized = 0;
        context->sensors[index_reset].transition_pending = 0;

        context->sensors[index_reset].contrast = 0;
        context->sensors[index_reset].approved = 0;
    }

    pwm_set_duty(PWM_A, 0);
    pwm_set_duty(PWM_B, 0);

    PORTB |= (1 << SENSOR_ON);
    PORTB |= (1 << LED_L);

    timer1_start();

    context->state_calibration = INIT_FRONT_CALIBRATION;

}

void calibration_front_start(CALIBRATION_CONTEXT *context){

    context->index = 1;

    pwm_set_duty(PWM_A, 0);
    pwm_set_duty(PWM_B, 0);

    PORTB &= ~(1 << MOTOR_L_IN1);
    PORTB |= (1 << MOTOR_L_IN2);

    PORTD &= ~(1 << MOTOR_R_IN2);
    PORTD |= (1 << MOTOR_R_IN1);

    if(MOTORS_KEY){

        pwm_set_duty(PWM_B, PERCENT_MOTOR_CALIBRATE);
        pwm_set_duty(PWM_A, PERCENT_MOTOR_CALIBRATE);
    }

    context->calibration_start_seconds = timer1_elapsed_seconds();

    context->state_calibration = FRONT_CALIBRATION;

}

void calibration_front_step(CALIBRATION_CONTEXT *context){

    uint8_t value_adc = 0;

    if((uint16_t)(timer1_elapsed_seconds() - context->calibration_start_seconds) >= CAL_TIMEOUT_SECONDS){

        pwm_set_duty(PWM_A, 0);
        pwm_set_duty(PWM_B, 0);
        context->state_calibration = CALIBRATION_FAULT;
        return;
    }

    SENSOR *sensor = &context->sensors[context->index];

    if(adc_read(sensor->channel, &value_adc)){

        if(!sensor->approved){

            sensor_calibration_update(sensor, value_adc);
            diagnostic_sensor_approved(sensor);
        }

        if(calibration_line_update(sensor->channel, value_adc, &context->white_seen, context->reference_white_count, context->reference_black_count)){

            context->initial_crossed_line = 1;
        }

        if(++context->index >= SENSORS_QUANTITY-1){

            context->index = 1;

            if(context->initial_crossed_line)context->state_calibration = INIT_LATERAL_CALIBRATION;
        }
    }

    uint8_t count_approved = 0;

    for(uint8_t index_conference = 1; index_conference < SENSORS_QUANTITY-1; index_conference++){

        if(context->sensors[index_conference].approved)count_approved++;
    }

    if(count_approved >= SENSORS_QUANTITY-2)context->state_calibration = INIT_LATERAL_CALIBRATION;

}

void calibration_lateral_start(CALIBRATION_CONTEXT *context){

    context->index = 0;
    context->white_seen = 0;
    context->initial_crossed_line = 0;

    for(uint8_t channel = 0; channel < SENSORS_QUANTITY; channel++){

        context->reference_white_count[channel] = 0;
        context->reference_black_count[channel] = 0;
    }

    context->calibration_start_seconds = timer1_elapsed_seconds();
    context->state_calibration = LATERAL_CALIBRATION;

}

void calibration_lateral_step(CALIBRATION_CONTEXT *context){

    uint8_t value_adc = 0;

    if((uint16_t)(timer1_elapsed_seconds() - context->calibration_start_seconds) >= CAL_TIMEOUT_SECONDS){

        pwm_set_duty(PWM_A, 0);
        pwm_set_duty(PWM_B, 0);
        context->state_calibration = CALIBRATION_FAULT;
        return;
    }

    SENSOR *sensor = &context->sensors[context->index];

    if(adc_read(sensor->channel, &value_adc)){

        if(!sensor->approved){

            sensor_calibration_update(sensor, value_adc);
            diagnostic_sensor_approved(sensor);
        }

        if(calibration_line_update(sensor->channel, value_adc, &context->white_seen, context->reference_white_count, context->reference_black_count)){

            context->initial_crossed_line = 1;
        }
        context->index = (context->index == QTR00_L) ? QTR07_L : QTR00_L;
    }

    if((context->initial_crossed_line && context->index == 0) || (context->sensors[QTR00_L].approved && context->sensors[QTR07_L].approved)){

        pwm_set_duty(PWM_A, 0);
        pwm_set_duty(PWM_B, 0);
        context->state_calibration = DIAGNOSTIC;
    }

}

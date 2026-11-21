/* Verificação, recuperação seletiva e conclusão da calibração. */

#include <avr/io.h>

#include "calibration_internal.h"
#include "pwm_lib.h"
#include "timer_lib.h"

void calibration_diagnostic_step(CALIBRATION_CONTEXT *context, STATE *state){

    switch(context->state_diagnostic){

    case DIAGNOSTIC_CHECK: {

        pwm_set_duty(PWM_A, 0);
        pwm_set_duty(PWM_B, 0);

        uint8_t front_reference = 0;

        for(uint8_t channel = 0; channel < SENSORS_QUANTITY; channel++){

            SENSOR *sensor = &context->sensors[channel];

            if(sensor->approved){

                if(channel >= QTR01_F && channel <= QTR06_F)front_reference = 1;

            }else{

                context->recalibration_mask |= (1U << channel);

                sensor->min = 255;
                sensor->max = 0;
                sensor->value_mean = 0;
                sensor->count_mean = 0;
                sensor->candidate_mean = 0;
                sensor->mean_initialized = 0;
                sensor->transition_pending = 0;
                sensor->contrast = 0;

            }
        }

        if(!context->recalibration_mask){

            context->state_diagnostic = DIAGNOSTIC_COMPLETE;
            break;
        }

        if(!front_reference){

            context->state_calibration = CALIBRATION_FAULT;
            break;
        }

        context->lateral_stop_seconds = timer1_elapsed_seconds();
        context->state_diagnostic = DIAGNOSTIC_LATERAL_RECALIBRATION;
        break;
    }

    case DIAGNOSTIC_FRONT_RECALIBRATION: {

        if(!context->recalibration_initialized){

            if((uint16_t)(timer1_elapsed_seconds() - context->lateral_stop_seconds) < MOTOR_REVERSE_DELAY_SECONDS)break;

            PORTB &= ~(1 << MOTOR_L_IN1);
            PORTB |= (1 << MOTOR_L_IN2);
            PORTD &= ~(1 << MOTOR_R_IN2);
            PORTD |= (1 << MOTOR_R_IN1);

            context->index = 0;

            for(uint8_t channel = 0; channel < SENSORS_QUANTITY; channel++)context->reference_white_count[channel] = 0;

            if(MOTORS_KEY){

                pwm_set_duty(PWM_A, PERCENT_MOTOR_CALIBRATE);
                pwm_set_duty(PWM_B, PERCENT_MOTOR_CALIBRATE);
            }

            context->calibration_start_seconds = timer1_elapsed_seconds();
            context->recalibration_initialized = 1;
        }

        if((uint16_t)(timer1_elapsed_seconds() - context->calibration_start_seconds) >= CAL_TIMEOUT_SECONDS){

            pwm_set_duty(PWM_A, 0);
            pwm_set_duty(PWM_B, 0);
            context->state_calibration = CALIBRATION_FAULT;
            break;
        }

        if(!diagnostic_recalibration_update(context->sensors, context->recalibration_mask, 1, &context->index, context->reference_white_count))break;

        pwm_set_duty(PWM_A, 0);
        pwm_set_duty(PWM_B, 0);

        for(uint8_t channel = QTR01_F; channel <= QTR06_F; channel++){

            if(diagnostic_sensor_approved(&context->sensors[channel]))context->recalibration_mask &= ~(1U << channel);
        }

        context->recalibration_initialized = 0;
        context->lateral_stop_seconds = timer1_elapsed_seconds();

        if(++context->recalibration_count >= RECALIBRATION_CYCLES || !context->recalibration_mask){

            context->state_diagnostic = DIAGNOSTIC_COMPLETE;
        }else{

            context->state_diagnostic = DIAGNOSTIC_LATERAL_RECALIBRATION;
        }
        break;
    }

    case DIAGNOSTIC_LATERAL_RECALIBRATION: {

        if(!context->recalibration_initialized){

            if((uint16_t)(timer1_elapsed_seconds() - context->lateral_stop_seconds) < MOTOR_REVERSE_DELAY_SECONDS)break;

            PORTB &= ~(1 << MOTOR_L_IN2);
            PORTB |= (1 << MOTOR_L_IN1);
            PORTD &= ~(1 << MOTOR_R_IN1);
            PORTD |= (1 << MOTOR_R_IN2);

            context->index = 0;

            for(uint8_t channel = 0; channel < SENSORS_QUANTITY; channel++)context->reference_white_count[channel] = 0;

            if(MOTORS_KEY){

                pwm_set_duty(PWM_A, PERCENT_MOTOR_CALIBRATE);
                pwm_set_duty(PWM_B, PERCENT_MOTOR_CALIBRATE);
            }

            context->calibration_start_seconds = timer1_elapsed_seconds();
            context->recalibration_initialized = 1;
        }

        if((uint16_t)(timer1_elapsed_seconds() - context->calibration_start_seconds) >= CAL_TIMEOUT_SECONDS){

            pwm_set_duty(PWM_A, 0);
            pwm_set_duty(PWM_B, 0);
            context->state_calibration = CALIBRATION_FAULT;
            break;
        }

        if(!diagnostic_recalibration_update(context->sensors, context->recalibration_mask, 0, &context->index, context->reference_white_count))break;

        pwm_set_duty(PWM_A, 0);
        pwm_set_duty(PWM_B, 0);

        context->recalibration_initialized = 0;
        context->lateral_stop_seconds = timer1_elapsed_seconds();

        uint8_t left_approved = diagnostic_sensor_approved(&context->sensors[QTR00_L]);
        uint8_t right_approved = diagnostic_sensor_approved(&context->sensors[QTR07_L]);

        if(left_approved)context->recalibration_mask &= ~(1U << QTR00_L);

        if(right_approved)context->recalibration_mask &= ~(1U << QTR07_L);

        if(!left_approved && !right_approved){

            context->state_calibration = CALIBRATION_FAULT;
            break;
        }

        context->state_diagnostic = DIAGNOSTIC_FRONT_RECALIBRATION;
        break;
    }

    case DIAGNOSTIC_COMPLETE: {

        pwm_set_duty(PWM_A, 0);
        pwm_set_duty(PWM_B, 0);

        uint8_t all_approved = 1;

        for(uint8_t channel = 0; channel < SENSORS_QUANTITY; channel++){

            if(!context->sensors[channel].approved)all_approved = 0;
        }

        if(!all_approved){

            context->state_calibration = CALIBRATION_FAULT;
            break;
        }

        PORTB &= ~(1 << LED_L);
        timer1_stop();
        *state = WAITING;
        break;
    }

    default:

        context->state_calibration = CALIBRATION_FAULT;
        break;
    }

}

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include "pegasus_config.h"
#include "adc_lib.h"
#include "pwm_lib.h"
#include "timer_lib.h"
#include "calibration.h"

#ifdef PEGASUS_SUPERVISOR
#include "supervisor.h"
#endif

static void init_sys(void){

    DDRD = (DDRD & ~(1 << BUTTON)) | (1 << MOTOR_R_IN1) | (1 << MOTOR_R_IN2) | (1 << MOTOR_L_PWM) | (1 << MOTOR_R_PWM);
    DDRB |= (1 << MOTOR_L_IN1) | (1 << MOTOR_L_IN2) | (1 << LED_L) | (1 << SENSOR_ON);

    PORTD &= ~(1 << BUTTON);
    PORTB &= ~(1 << LED_L);
    PORTB &= ~(1 << SENSOR_ON);

    adc_init();
    pwm_init();
    timer1_ctc_init();
    timer1_interrupt_enable();
    timer1_start();
    #ifdef PEGASUS_SUPERVISOR
    supervisor_init();
    #endif
    sei();
}

int main(void){

    init_sys();

    STATE state = INIT;

    uint16_t time_init = 0;
    uint16_t time_change = 0;

    uint8_t button_last = 0;
    uint8_t button_stable = 0;
    uint8_t button_handled = 0;

    for(;;){

        #ifdef PEGASUS_SUPERVISOR
        supervisor_step(&state);
        #endif

        uint16_t now = timer1_elapsed_milliseconds();
        uint8_t button = (PIND >> BUTTON) & 1U;

        if(state == INIT || state == WAITING || state == EMERGENCY){

            timer1_start();

            if(button != button_last){

                time_change = now;
                button_last = button;
            }

            if((uint16_t)(now - time_change) >= BUTTON_DEBOUNCE_MS && button != button_stable){

                button_stable = button;

                if(button_stable){

                    time_init = time_change;
                    button_handled = 0;

                }else if(!button_handled){

                    button_handled = 1;

                    if((uint16_t)(time_change - time_init) < BUTTON_TIMEOUT_SECONDS * 1000U){

                        calibration_reset();
                        state = CALIBRATION;
                        PORTB |= (1 << LED_L);

                    }else{

                        state = WAITING;
                    }
                }
            }

            if(button_stable && button && !button_handled && (uint16_t)(now - time_init) >= BUTTON_TIMEOUT_SECONDS * 1000U){

                button_handled = 1;
                state = WAITING;
            }

        }else{

            button_last = button;
            button_stable = button;
            button_handled = 1;
            time_change = now;
        }

        switch(state){

        case CALIBRATION:

            calibration_sensor(&state);
            break;

        case EMERGENCY:

            calibration_sensor(&state);
            break;

        case INIT:

        case WAITING:

        case RUNNING:

        case FINISH:

        case STOP:

        default:

            break;
        }
        #ifdef PEGASUS_SUPERVISOR
        supervisor_after(&state);
        #endif
    }

}

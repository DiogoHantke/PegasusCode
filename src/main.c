#include <avr/io.h>
#include <avr/interrupt.h>

#include "pegasus_config.h"
#include "adc_lib.h"
#include "pwm_lib.h"
#include "timer_lib.h"
#include "calibration.h"
#include "line_follower.h"
#include "pegasus_system.h"

#ifdef PEGASUS_SUPERVISOR
#include "supervisor.h"
#endif

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

static void stop_motors(void){

    pwm_set_duty(PWM_A, 0);
    pwm_set_duty(PWM_B, 0);

}

int main(void){

    init_sys();

    STATE state = INIT;

    static uint16_t time_init;
    static uint16_t time_change;

    static uint8_t button_last;
    static uint8_t button_stable;
    static uint8_t button_handled = 1;
    static uint8_t button_armed;

    for(;;){

        timer1_start();

        if(state != RUNNING)line_follower_reset();

#ifdef PEGASUS_SUPERVISOR
        supervisor_step(&state);
#endif
        uint16_t now = timer1_elapsed_milliseconds();
        uint8_t button = (PIND >> BUTTON) & 1U;
        uint8_t pressed = 0;
        uint8_t released = 0;
        uint8_t allowed = 1;

#ifdef PEGASUS_SUPERVISOR
        allowed = supervisor_button_allowed();
#endif

        if(!allowed){
            button_armed = 0;
            button_handled = 1;
        }

        if(button != button_last){
            time_change = now;
            button_last = button;
        }

        if((uint16_t)(now - time_change) >= BUTTON_DEBOUNCE_MS && button != button_stable){
            button_stable = button;
            if(button_stable){
                if(allowed && button_armed){
                    pressed = 1;
                    time_init = time_change;
                    button_handled = 0;
                }
            }else{
                released = allowed && button_armed;
            }
        }
        if(allowed && !button && !button_stable){

            button_armed = 1;

        }
        uint8_t emergency_active = state == EMERGENCY;
#ifdef PEGASUS_SUPERVISOR
        emergency_active |= supervisor_emergency_active();
#endif
        if(emergency_active){

            led_emergency_update();

        }else{

            led_emergency_reset();

        }

        switch(state){

        case INIT:
            if(released && !button_handled){
                button_handled = 1;
                if((uint16_t)(time_change - time_init) < BUTTON_TIMEOUT_SECONDS * 1000U){

                    calibration_reset();
                    state = CALIBRATION;
                    PORTB |= (1 << LED_L);

                }else{
                    state = WAITING;
                }
            }

            if(allowed && button_armed && button_stable && button && !button_handled &&
               (uint16_t)(now - time_init) >= BUTTON_TIMEOUT_SECONDS * 1000U){

                button_handled = 1;
                state = WAITING;

            }
            
            break;

        case CALIBRATION:
            calibration_sensor(&state);
            break;

        case WAITING:
            if(pressed){
                state = RUNNING;
            }

            break;

        case RUNNING:
            if(pressed){
                line_follower_reset();
                state = STOP;
            }else{
                line_follower_step(&state);
            }
            break;

        case FINISH:

            stop_motors();

            if(pressed){
                button_handled = 1;
                state = INIT;
            }

            break;

        case STOP:

            stop_motors();

            if(pressed){
                button_handled = 1;
                state = INIT;
            }

            break;

        case EMERGENCY:

            stop_motors();
            PORTB &= ~(1 << SENSOR_ON);

            if(pressed){
                button_handled = 1;
                state = INIT;
            }

            break;

        default:
            break;
        }

        if(state != INIT){
            button_handled = 1;
        }

#ifdef PEGASUS_SUPERVISOR
        supervisor_after(&state);
#endif
        if(state != RUNNING)line_follower_reset();

    }
}

#include <avr/io.h>
#include <avr/interrupt.h>

#include "pegasus_config.h"
#include "adc_lib.h"
#include "pwm_lib.h"
#include "timer_lib.h"
#include "calibration.h"

int main(void){

    init_sys();

    STATE state = INIT;

    static uint32_t time_init = 0;
    static uint32_t time_change = 0;

    static uint8_t button_last = 0;
    static uint8_t button_stable = 0;

    for(;;){

        switch(state){
            
        case INIT: {

            uint32_t now = timer1_elapsed_milliseconds();
            uint8_t button = (PIND >> BUTTON) & 1U;

            if(button != button_last){
                time_change = now;
                button_last = button;
            }

            if((uint32_t)(now - time_change) >= 500UL && button != button_stable){

                button_stable = button;

                if(button_stable){
                    time_init = time_change;

                }else if((uint32_t)(time_change - time_init) < BUTTON_TIMEOUT_SECONDS * 1000UL){
                    state = CALIBRATION;
                    PORTB |= (1 << LED_L);

                }else{
                    state = WAITING;
                }
            }

            if(button_stable && button && (uint32_t)(now - time_init) >= BUTTON_TIMEOUT_SECONDS * 1000UL){

                state = WAITING;
            }

            break;
        }

        case CALIBRATION:
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

}

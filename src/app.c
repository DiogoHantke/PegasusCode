#include <avr/io.h>
#include "app.h"
#include "button.h"
#include "buzzer.h"
#include "calibration.h"
#include "line_follower.h"
#include "pegasus_config.h"
#include "pegasus_system.h"
#include "pwm_lib.h"
#include "timer_lib.h"
#include "status_monitor.h"

static STATE state;
static uint8_t calibrated, consumed;

static uint8_t button_raw(void){
    return !!(PIND & (1 << BUTTON)) == BUTTON_ACTIVE_HIGH;
}

static void stop_motors(void){
    pwm_set_raw(PWM_A, 0);
    pwm_set_raw(PWM_B, 0);
}

static void enter_state(STATE next){

    if(next == state)return;
    if(state == RUNNING)line_follower_reset();
    stop_motors();
    state = next;
    if(next == CALIBRATION){
        calibration_reset();
        calibrated = 0;
        buzzer_beep(1);
    }else if(next == WAITING){
        calibrated = 1;
        buzzer_beep(calibration_front_mask() == 0x7EU ? 2 : 3);
    }else{
        buzzer_beep(next == EMERGENCY ? 3 : 1);
    }
    if(next != CALIBRATION && next != RUNNING)PORTB &= ~(1 << SENSOR_ON);

}

STATE app_state(void){ return state; }

void app_init(void){

    init_sys();
    line_follower_reset();
    stop_motors();
    state = INIT;
    calibrated = consumed = 0;
    button_init(timer1_elapsed_milliseconds(), button_raw());
    buzzer_beep(1);

}

void app_step(void){

    uint16_t now = timer1_elapsed_milliseconds();
    uint8_t events = button_update(now, button_raw());

    if(events & BUTTON_PRESS){
        consumed = 0;
        if(state == RUNNING || state == CALIBRATION){
            consumed = 1;
            enter_state(STOP);
        }
    }
    if(events & BUTTON_LONG){
        enter_state(CALIBRATION);
    }else if((events & BUTTON_SHORT) && !consumed){
        if(!calibrated)enter_state(CALIBRATION);
        else enter_state(RUNNING);
    }

    if(state == CALIBRATION){
        STATE result = state;
        calibration_sensor(&result);
        if(result != state)enter_state(result);
    }else if(state == RUNNING){
        STATE result = state;
        line_follower_step(&result);
        if(result != state)enter_state(result);
    }else{
        stop_motors();
    }

    uint8_t led;
    if(state == EMERGENCY)led = now % 1000U < 150U;
    else if(state == CALIBRATION)led = now % 200U < 100U;
    else led = state == WAITING || state == RUNNING || calibrated;
    if(led)PORTB |= 1 << LED_L;
    else PORTB &= ~(1 << LED_L);
    buzzer_update();
    status_monitor_step(state, button_raw());

}

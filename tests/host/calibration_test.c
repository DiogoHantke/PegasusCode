#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include "calibration.h"
#include "pegasus_config.h"
#include "pwm_lib.h"

uint8_t DDRD, DDRB, PORTD, PORTB, PIND;
static uint16_t seconds, last_stop_seconds;
static unsigned samples[8], tick, leg_start_tick, legs, initial_legs;
static uint8_t failed_mask, recover, travel, front_timeout, no_adc, noisy, delayed;
static uint8_t diagnostic_started, lateral_started, timer_stopped;
static uint8_t initial_mode;
static uint8_t pwm[2];

void adc_init(void){}
uint8_t adc_read(uint8_t channel, uint8_t *value){

    static uint8_t pending, active_channel;
    if(initial_mode == 3)return 0;
    if(initial_mode == 4 && (channel == QTR00_L || channel == QTR07_L)){
        lateral_started = 1;
        return 0;
    }
    if(no_adc && diagnostic_started)return 0;
    if(!pending){
        pending = 1;
        active_channel = channel;
        return 0;
    }
    pending = 0;
    if(active_channel != channel)return 0;

    if(!diagnostic_started){
        uint8_t lateral = channel == QTR00_L || channel == QTR07_L;
        if(lateral)lateral_started = 1;

        assert(!lateral_started || lateral);
        assert(pwm[0] && pwm[1]);
        uint8_t white = samples[channel]++ < 30;
        if(initial_mode == 1)white = 1;
        if(initial_mode == 2)white = 0;
        uint8_t low = failed_mask & (1U << channel) ? 120 : 20;
        uint8_t high = failed_mask & (1U << channel) ? 136 : 200;
        *value = white == CAL_WHITE_HIGH ? high : low;
        return 1;
    }

    assert(legs && pwm[0] && pwm[1]);
    uint8_t front = !(legs & 1U);
    uint8_t lateral = channel == QTR00_L || channel == QTR07_L;
    uint8_t reference = front ? lateral : !lateral;
    unsigned elapsed = tick - leg_start_tick;

    if(reference){

        assert(!(failed_mask & (1U << channel)) || (recover && lateral && legs >= (delayed ? 6U : 2U)));
        uint8_t white = travel && elapsed >= 1200 && !(front_timeout && front);
        if(noisy && elapsed >= 100 && elapsed < 115)white = 1;
        *value = white == CAL_WHITE_HIGH ? 200 : 20;
    }else{
        assert(failed_mask & (1U << channel));
        if(!recover || (delayed && legs < 5))*value = 100;
        else if(elapsed < 400)*value = delayed ? 10 : 20;
        else *value = delayed ? 230 : 200;
    }
    return 1;
}
void pwm_init(void){}
void pwm_set_duty(pwm_channel_t channel, uint8_t duty){
    uint8_t was_moving = pwm[0] && pwm[1];
    pwm[channel] = duty;
    uint8_t moving = pwm[0] && pwm[1];
    if(was_moving && !moving){
        last_stop_seconds = seconds;
        if(!diagnostic_started && !initial_mode){
            assert(lateral_started);
            diagnostic_started = 1;
        }
    }
    if(!was_moving && moving){
        uint8_t front;
        if(diagnostic_started){
            assert((uint16_t)(seconds - last_stop_seconds) >= MOTOR_REVERSE_DELAY_SECONDS);
            legs++;
            leg_start_tick = tick;
            front = !(legs & 1U);
        }else{
            initial_legs++;
            front = 1;
        }

        if(front){
            assert((PORTB & (1 << MOTOR_L_IN2)) && !(PORTB & (1 << MOTOR_L_IN1)));
            assert(!(PORTD & (1 << MOTOR_R_IN2)) && (PORTD & (1 << MOTOR_R_IN1)));
        }else{
            assert(!(PORTB & (1 << MOTOR_L_IN2)) && (PORTB & (1 << MOTOR_L_IN1)));
            assert((PORTD & (1 << MOTOR_R_IN2)) && !(PORTD & (1 << MOTOR_R_IN1)));
        }
    }
    if(was_moving && !moving && legs && travel && !(front_timeout && !(legs & 1U))){
        assert(tick - leg_start_tick >= 1200);
    }
}
void timer1_ctc_init(void){}
void timer1_interrupt_enable(void){}
void timer1_start(void){ timer_stopped = 0; }
void timer1_stop(void){
    assert(!pwm[0] && !pwm[1]);
    timer_stopped = 1;
}
uint16_t timer1_elapsed_seconds(void){ return seconds; }

int main(int argc, char **argv){
    assert(argc == 2);
    const char *mode = argv[1];
    if(!strcmp(mode, "initial-white"))initial_mode = 1;
    if(!strcmp(mode, "initial-black"))initial_mode = 2;
    if(!strcmp(mode, "initial-no-adc"))initial_mode = 3;
    if(!strcmp(mode, "initial-lateral-no-adc"))initial_mode = 4;
    if(!strcmp(mode, "front") || !strcmp(mode, "front-recovery") || !strcmp(mode, "noise") || !strcmp(mode, "no-adc") || !strcmp(mode, "front-timeout") || !strcmp(mode, "contrast-failure"))failed_mask = 1U << 3;
    if(!strcmp(mode, "lateral") || !strcmp(mode, "lateral-recovery"))failed_mask = 1U;
    if(!strcmp(mode, "both-recovery") || !strcmp(mode, "delayed-recovery"))failed_mask = (1U << 3) | 1U;
    if(!strcmp(mode, "no-front-reference"))failed_mask = 0x7E;
    if(!strcmp(mode, "no-lateral-reference") || !strcmp(mode, "all-lateral-recovery"))failed_mask = 0x81;
    recover = strstr(mode, "recovery") != NULL || !strcmp(mode, "noise");
    travel = recover || !strcmp(mode, "contrast-failure") || !strcmp(mode, "front-timeout") || !strcmp(mode, "no-lateral-reference");
    delayed = !strcmp(mode, "delayed-recovery");
    front_timeout = !strcmp(mode, "front-timeout");
    no_adc = !strcmp(mode, "no-adc");
    noisy = !strcmp(mode, "noise");

    uint8_t repeat = !strcmp(mode, "repeat-success") || !strcmp(mode, "repeat-fault");
    if(!strcmp(mode, "repeat-fault"))initial_mode = 3;
    STATE state = CALIBRATION;
    calibration_sensor(NULL);
    calibration_reset();
restart:
    unsigned led_changes = 0;
    uint8_t last_led = 0;
    for(tick = 0; tick < 60000; tick++){
        seconds = tick / 1000;
        if(state == CALIBRATION || state == EMERGENCY)calibration_sensor(&state);
        uint8_t led = !!(PORTB & (1 << LED_L));
        if(led != last_led)led_changes++;
        last_led = led;
    }
    assert(!pwm[0] && !pwm[1] && initial_legs == 1);
    if(repeat){
        assert(state == (initial_mode ? EMERGENCY : WAITING));
        repeat = 0;
        initial_mode = 0;
        diagnostic_started = 0;
        lateral_started = 0;
        initial_legs = 0;
        memset(samples, 0, sizeof samples);
        calibration_reset();
        state = CALIBRATION;
        goto restart;
    }
    if(initial_mode){
        assert(state == EMERGENCY && !timer_stopped && led_changes > 2);
        assert(!legs);
        assert(lateral_started == (initial_mode == 4));
        printf("PASS %s: no false line transition, bounded movement and blinking fault LED\n", mode);
        return 0;
    }
    uint8_t success = recover || !failed_mask;
    assert(state == (success ? WAITING : EMERGENCY));
    if(success){
        assert(timer_stopped && !(PORTB & (1 << LED_L)) && led_changes == 2);
    }else{
        assert(!timer_stopped && led_changes > 2 && !(PORTB & (1 << SENSOR_ON)));
    }
    if(delayed || !strcmp(mode, "contrast-failure"))assert(legs == RECALIBRATION_CYCLES * 2U);
    else if(recover || front_timeout)assert(legs == 2);
    else if(!strcmp(mode, "front") || !strcmp(mode, "lateral") || no_adc || !strcmp(mode, "no-lateral-reference"))assert(legs == 1);
    else assert(!legs);

    printf("PASS calibration %s: continuous advance, %u diagnostic legs and correct final state\n", mode, legs);
}

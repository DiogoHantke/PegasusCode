#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "pegasus_config.h"
#include "pwm_lib.h"

#define main firmware_main
#include "../../src/main.c"
#undef main

uint8_t DDRD, DDRB, PORTD, PORTB, PIND;
static jmp_buf finished;
static unsigned loops;
static uint16_t now;
static uint8_t left, right, calibrated, saw_running, losing;

void init_sys(void){}
void calibration_reset(void){}
void calibration_sensor(STATE *current){
    calibrated = 1;
    *current = WAITING;
}
void calibration_get(uint8_t channel, uint8_t *low, uint8_t *high, uint8_t *ok){
    (void)channel;
    *low = 20;
    *high = 220;
    *ok = calibrated;
}
uint16_t timer1_elapsed_milliseconds(void){ return now; }
void timer1_start(void){
    now = ++loops / 32;
    uint8_t pressing = (now >= 100 && now < 800) ||
                       (now >= 2000 && now < 2700) ||
                       (!losing && now >= 4000 && now < 4700);
    PIND = pressing << BUTTON;
    if(now >= 2800 && now < 3000){
        assert(calibrated && left == LINE_BASE_SPEED && right == LINE_BASE_SPEED);
        saw_running = 1;
    }
    if(now >= 5000){
        assert(saw_running && !left && !right);
        assert(!(PORTB & (1 << SENSOR_ON)));
    }
    if(now >= 6000)longjmp(finished, 1);
}
uint8_t adc_read_10(uint8_t channel, uint16_t *value){
    static uint8_t pending;
    pending = !pending;
    if(pending)return 0;
    uint8_t black = channel == 3 || channel == 4;
    if(losing && now >= 3000)black = 0;
    *value = (black != CAL_WHITE_HIGH) ? 880 : 80;
    if(channel == QTR00_L)*value = 1023;
    if(channel == QTR07_L)*value = 0;
    return 1;
}
void pwm_set_raw(pwm_channel_t channel, uint8_t value){
    assert(value <= MOTOR_PWM_MAX);
    if(channel == PWM_B)left = value;
    else right = value;
}
void pwm_set_duty(pwm_channel_t channel, uint8_t value){
    assert(value == 0);
    pwm_set_raw(channel, 0);
}
int main(int argc, char **argv){
    assert(argc == 2);
    losing = !strcmp(argv[1], "loss");
    if(!setjmp(finished))firmware_main();
    puts("main: original calibration/start buttons, running and stop OK");
}

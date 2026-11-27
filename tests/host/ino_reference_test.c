/* Compara a corrida completa com as expressoes copiadas de Main (1).ino. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "pegasus_config.h"
#include "line_follower.h"
#include "pwm_lib.h"

/* Testa ambas as polaridades sem alterar a configuracao original da calibracao. */
uint8_t DDRD, DDRB, PORTD, PORTB, PIND;
static uint16_t now, values[8];
static uint8_t pending, read_index, left, right;
static const uint8_t order[8] = {6,5,4,3,2,1,7,0};
static int last_error;

uint16_t timer1_elapsed_milliseconds(void){ return now; }
void calibration_get(uint8_t channel, uint8_t *low, uint8_t *high, uint8_t *ok){
    (void)channel;
    *low = 10;
    *high = 230;
    *ok = 1;
}
uint8_t adc_read_10(uint8_t channel, uint16_t *raw){
    assert(channel == order[read_index]);
    pending = !pending;
    if(pending)return 0;
    *raw = values[channel];
    read_index = (read_index + 1) % 8;
    return 1;
}
void pwm_set_raw(pwm_channel_t channel, uint8_t value){
    assert(value <= MOTOR_PWM_MAX);
    if(channel == PWM_B)left = value;
    else right = value;
}
void pwm_set_duty(pwm_channel_t channel, uint8_t value){
    assert(value == 0);
    pwm_set_raw(channel,0);
}
static int position_reference(void){
    int s[6], sum = 0;
    for(unsigned i = 0; i < 6; i++){
        int raw = values[6-i];
        if(raw < 40)raw = 40;
        if(raw > 920)raw = 920;
        s[i] = (raw - 40) * 100 / (920 - 40);
        if(CAL_WHITE_HIGH)s[i] = 100 - s[i];
        sum += s[i];
    }
    assert(sum > 100);
    int prom = -2.5*s[0] -1.5*s[1] -0.5*s[2] +0.5*s[3] +1.5*s[4] +2.5*s[5];
    return (int)(100.0 * prom / sum);
}
static int pid_reference(int position){
    int error = position;
    int derivative = error - last_error;
    last_error = error;
    int pot_giro = error * LINE_KP + derivative * LINE_KD;
    if(pot_giro > LINE_TURN_LIMIT)pot_giro = LINE_TURN_LIMIT;
    else if(pot_giro < -LINE_TURN_LIMIT)pot_giro = -LINE_TURN_LIMIT;
    return pot_giro;
}
static int constrain_motor(int value){
    if(value > (int)MOTOR_PWM_MAX)return MOTOR_PWM_MAX;
    if(value < -(int)MOTOR_PWM_MAX)return -(int)MOTOR_PWM_MAX;
    return value;
}
int main(void){
    STATE state = RUNNING;
    uint32_t seed = 37;
    values[0] = 1023; /* A0 fora da marca. */
    values[7] = 0;    /* Sem evento Curve_line. */
    for(unsigned frame = 0; frame < 1000; frame++){
        for(unsigned c = 1; c <= 6; c++){
            seed = seed * 1664525UL + 1013904223UL;
            values[c] = (seed >> 16) % 1024;
        }
        values[1 + frame % 6] = CAL_WHITE_HIGH ? 40 : 920;
        values[1 + (frame + 1) % 6] = CAL_WHITE_HIGH ? 40 : 920;
        int correction = pid_reference(position_reference());
        int expected_left = constrain_motor(LINE_BASE_SPEED - correction);
        int expected_right = constrain_motor(LINE_BASE_SPEED + correction);
        now += LINE_PERIOD_MS;
        for(unsigned call = 0; call < 16; call++)line_follower_step(&state);
        assert(state == RUNNING);
        int actual_left = PORTB & (1 << MOTOR_L_IN2) ? left : -(int)left;
        int actual_right = PORTD & (1 << MOTOR_R_IN1) ? right : -(int)right;
        assert(actual_left == expected_left && actual_right == expected_right);
        assert(!!(PORTB & (1 << MOTOR_L_IN1)) != !!(PORTB & (1 << MOTOR_L_IN2)));
        assert(!!(PORTD & (1 << MOTOR_R_IN1)) != !!(PORTD & (1 << MOTOR_R_IN2)));
    }
    puts("sketch position/PID and physical motor mapping: 1000 frames OK");
}

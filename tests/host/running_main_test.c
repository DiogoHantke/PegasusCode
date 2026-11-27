#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include "app.h"
#include "buzzer.h"
#include "calibration.h"
#include "pegasus_config.h"
#include "pwm_lib.h"

uint8_t DDRD, DDRB, PORTD, PORTB, PIND;
uint8_t TCCR2A, TCCR2B, TCNT2, OCR2A, TIMSK2, TIFR2;
static uint16_t now;
static uint8_t left, right, fail_mask, no_adc, losing;
static unsigned beeps;

uint16_t timer1_elapsed_milliseconds(void){ return now; }
void init_sys(void){ buzzer_init(); }
void status_monitor_step(STATE state, uint8_t button){ (void)state; (void)button; }
void pwm_set_raw(pwm_channel_t channel, uint8_t value){
    assert(value <= MOTOR_PWM_MAX);
    if(channel == PWM_B)left = value;
    else right = value;
}
void pwm_set_duty(pwm_channel_t channel, uint8_t percent){
    assert(percent == 0);
    pwm_set_raw(channel, 0);
}
uint8_t adc_read_10(uint8_t channel, uint16_t *value){
    static uint8_t pending;
    if(no_adc)return 0;
    pending = !pending;
    if(pending)return 0;
    uint8_t white;
    if(app_state() == CALIBRATION)white = (now / 400) & 1U;
    else white = channel == 3 || channel == 4;
    if(losing)white = 0;
    *value = (white == CAL_WHITE_HIGH) ? 880 : 80;
    if(app_state() != CALIBRATION){
        if(channel == 0)*value = 1023;
        if(channel == 7)*value = 0;
    }
    if(fail_mask & (1U << channel))*value = 0;
    return 1;
}
uint8_t adc_read(uint8_t channel, uint8_t *value){
    uint16_t raw;
    if(!adc_read_10(channel, &raw))return 0;
    *value = raw >> 2;
    return 1;
}
static void step(unsigned duration, uint8_t down){
    while(duration--){
        now++;
        PIND = down << BUTTON;
        for(unsigned calls = 0; calls < 32; calls++)app_step();
        if(TCCR2B)beeps++;
    }
}
static void tap(void){ step(60, 1); step(40, 0); }
int main(int argc, char **argv){
    assert(argc == 2);
    if(!strcmp(argv[1], "partial"))fail_mask = 0x87U;
    if(!strcmp(argv[1], "one"))fail_mask = 0xFFU & ~(1U << 3);
    if(!strcmp(argv[1], "none"))fail_mask = 0xFFU;
    if(!strcmp(argv[1], "no-adc"))no_adc = 1;
    if(!strcmp(argv[1], "wrap"))now = 65000;
    app_init();
    step(300, 0);
    assert(app_state() == INIT && !left && !right && beeps);
    step(2100, 1);
    assert(app_state() == CALIBRATION);
    step(40, 0);
    step(CAL_LEG_MS * 2 + CAL_REVERSE_PAUSE_MS + 100, 0);
    if(no_adc || fail_mask == 0xFFU){
        assert(app_state() == EMERGENCY && !left && !right);
        no_adc = fail_mask = 0;
        tap(); /* Falha de calibracao pode ser repetida com um toque. */
        step(CAL_LEG_MS * 2 + CAL_REVERSE_PAUSE_MS + 100, 0);
    }
    assert(app_state() == WAITING && !left && !right);
    assert(calibration_front_mask());
    tap();
    step(100, 0);
    assert(app_state() == RUNNING && (left || right));
    step(40, 1);
    assert(app_state() == STOP && !left && !right);
    step(40, 0);
    assert(app_state() == STOP); /* Soltar nao reinicia. */
    tap();
    step(100, 0);
    assert(app_state() == RUNNING && (left || right));
    losing = 1;
    step(LINE_LOST_TIMEOUT_MS + 10, 0);
    assert(app_state() == EMERGENCY && !left && !right);
    losing = 0;
    tap();
    step(100, 0);
    assert(app_state() == RUNNING); /* Sem ciclo INIT extra para recuperar. */
    step(2100, 1); /* Para imediatamente e depois recalibra. */
    assert(app_state() == CALIBRATION);
    step(40, 0);
    step(40, 1); /* Calibracao tambem pode ser cancelada. */
    assert(app_state() == STOP && !left && !right);
    step(40, 0);
    assert(app_state() == STOP);
    puts("app: button, buzzer, real calibration, partial sensors, running, stop and retry OK");
}

#include <avr/io.h>
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "pegasus_state.h"
#include "pegasus_config.h"

uint8_t DDRD, DDRB, PORTD, PORTB, PIND;

static jmp_buf finished;
static unsigned tick, starts, resets, first_start, phase;
static uint16_t milliseconds;
static uint8_t timer_running, wrapping, fail_first, retry, long_press, bounce, busy_press;

void adc_init(void){}
void pwm_init(void){}
void timer1_ctc_init(void){ milliseconds = wrapping ? 65000U : 0; }
void timer1_interrupt_enable(void){}
void timer1_start(void){ timer_running = 1; }

uint16_t timer1_elapsed_milliseconds(void){

    if(++tick >= 8000)longjmp(finished, 1);

    uint8_t pressed;

    if(bounce){

        pressed = tick >= 100 && tick < 110;

    }else if(long_press){

        pressed = tick >= 100 && tick < 2600;
        if(retry && tick >= 3000 && tick < 3100)pressed = 1;

    }else{

        pressed = tick >= 100 && tick < 200;
        if(retry && tick >= 1000 && tick < 1100)pressed = 1;
        if(busy_press && tick >= 300 && tick < 1200)pressed = 1;
        if(busy_press && tick >= 1500 && tick < 1600)pressed = 1;
    }

    PIND = pressed ? (1 << BUTTON) : 0;

    if(timer_running)milliseconds++;

    return milliseconds;

}

void calibration_reset(void){

    resets++;
    phase = 0;

}

void calibration_sensor(STATE *state){

    if(*state == EMERGENCY)return;

    if(!phase){

        starts++;
        if(!first_start)first_start = tick;
    }

    if(++phase >= (busy_press ? 500U : 4U)){

        *state = fail_first && starts == 1 ? EMERGENCY : WAITING;

        if(*state == WAITING)timer_running = 0;
    }

}

#define main pegasus_main
#include "../../src/main.c"
#undef main

int main(int argc, char **argv){

    assert(argc == 2);

    const char *scenario = argv[1];

    wrapping = strstr(scenario, "wrap") != NULL;
    long_press = strstr(scenario, "long") != NULL;
    retry = strstr(scenario, "retry") != NULL;
    fail_first = strstr(scenario, "fault") != NULL;
    bounce = !strcmp(scenario, "bounce");
    busy_press = !strcmp(scenario, "busy");

    if(!setjmp(finished))pegasus_main();

    unsigned expected = bounce || (long_press && !retry) ? 0U : retry && !long_press ? 2U : busy_press ? 2U : 1U;

    assert(starts == expected && resets == expected);
    assert(!first_start || first_start == (long_press ? 3130U : 230U));
    assert(timer_running);

    printf("PASS main %s: button debounce, timing and calibration requests\n", scenario);

}

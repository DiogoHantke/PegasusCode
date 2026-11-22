#include "pwm_lib.h"
#include <assert.h>
#include <avr/io.h>
#include <stdio.h>
#include <string.h>
uint8_t PORTB;
uint8_t PORTD;
uint8_t OCR0A;
uint8_t OCR0B;
uint8_t UCSR0A;
uint8_t UCSR0B;
uint8_t UCSR0C;
uint8_t UDR0;
uint8_t UBRR0H;
uint8_t UBRR0L;
static uint16_t now;
static uint8_t readings[7] = {0, 0, 128, 255, 42, 84, 252};
static char wire[2048];
static unsigned wire_length;

void pwm_set_duty(pwm_channel_t c, uint8_t percent){

    if(c == PWM_A)
        OCR0A = percent * 255U / 100;
    else
        OCR0B = percent * 255U / 100;

}

void timer1_start(void){

}

uint16_t timer1_elapsed_milliseconds(void){

    return now;

}

void calibration_reset(void){

}

uint8_t calibration_phase(void){

    return 0;

}

void calibration_get(uint8_t c, uint8_t *low, uint8_t *high, uint8_t *ok){

    (void)c;
    *low = 10;
    *high = 230;
    *ok = 1;

}

uint8_t adc_latest(uint8_t c){

    return readings[c];

}

uint8_t adc_valid_mask(void){

    return 126;

}

uint8_t adc_read(uint8_t c, uint8_t *v){

    *v = readings[c];
    return 1;

}

#include "../../src/communication/supervisor.c"

static void drain(void){

    while(tt != th){

        USART_UDRE_vect();
        assert(wire_length < sizeof(wire) - 1);
        wire[wire_length++] = UDR0;

    }

    wire[wire_length] = 0;

}

static void flush(STATE *state){

    for(uint8_t i = 0; i < 12; i++){

        supervisor_step(state);
        supervisor_after(state);
        drain();

    }

    assert(ack_code == NONE && !replies);

}

static void command(STATE *state, uint8_t seq, const char *body){

    char f[128];
    int n = snprintf(f, sizeof(f), "@%u,%s", seq, body);
    uint8_t crc = protocol_crc(f + 1, n - 1);
    snprintf(f + n, sizeof(f) - n, "*%02X\n", crc);
    wire_length = 0;
    wire[0] = 0;
    uint8_t count = 0;
    for(char *p = f; *p; p++){

        UDR0 = *p;
        USART_RX_vect();
        if(++count == 16){

            supervisor_step(state);
            drain();
            count = 0;

        }

    }

    flush(state);

}

static uint8_t reply_code(void){

    char *p = strstr(wire, ",ACK,");
    if(!p)
        p = strstr(wire, ",NACK,");
    assert(p);
    p = strchr(p + 1, ',');
    return (uint8_t)(p[1] - '0');

}

int main(int argc, char **argv){

    assert(argc == 2);
    STATE state = INIT;
    supervisor_init();
    assert(!OCR0A && !OCR0B);
    if(!strcmp(argv[1], "estop")){

        command(&state, 1, "MANUAL_ENABLE");
        command(&state, 2, "MANUAL_FORWARD,80");
        assert(OCR0A && OCR0B);
        command(&state, 1, "ESTOP");
        assert(estop && !manual && state == STOP && !OCR0A && !OCR0B);
        assert(supervisor_emergency_active());
        command(&state, 3, "MANUAL_ENABLE");
        assert(reply_code() == 4 && !manual);
        command(&state, 4, "REARM");
        assert(!estop && state == WAITING && !OCR0A);
        assert(!supervisor_emergency_active());

    } else if(!strcmp(argv[1], "deadman") || !strcmp(argv[1], "manual-heartbeat")){

        command(&state, 1, "MANUAL_ENABLE");
        command(&state, 2, "MANUAL_FORWARD,80");
        now = 200;
        command(&state, 2, "MANUAL_FORWARD,80");
        assert(reply_code() == 0);
        command(&state, 3, "PING");
        now = 301;
        supervisor_step(&state);
        assert(state == STOP && !manual && !OCR0A && fault == 2);

    } else if(!strcmp(argv[1], "wrap")){

        now = 65500;
        command(&state, 1, "MANUAL_ENABLE");
        command(&state, 2, "MANUAL_LEFT,80");
        now = 100;
        supervisor_step(&state);
        assert(manual && OCR0A);
        now = 300;
        supervisor_step(&state);
        assert(!manual && !OCR0A);

    } else if(!strcmp(argv[1], "interlocks")){

        command(&state, 1, "START");
        assert(state == INIT && reply_code() == 6 && !OCR0A);
        command(&state, 2, "CALIBRATE");
        assert(state == CALIBRATION);
        command(&state, 3, "MANUAL_ENABLE");
        assert(reply_code() == 3 && !manual);
        command(&state, 4, "STOP");
        assert(state == STOP && !OCR0A);
        command(&state, 5, "MANUAL_ENABLE");
        command(&state, 6, "MANUAL_RIGHT,999");
        assert(reply_code() == 2 && !OCR0A);
        command(&state, 7, "MANUAL_BACKWARD,160");
        assert(reply_code() == 0 && OCR0A && OCR0B);

    } else if(!strcmp(argv[1], "overflow")){

        command(&state, 1, "MANUAL_ENABLE");
        command(&state, 2, "MANUAL_FORWARD,80");
        for(uint8_t i = 0; i < 100; i++){

            UDR0 = 'x';
            USART_RX_vect();

        }

        supervisor_step(&state);
        assert(state == STOP && !OCR0A && !manual && fault == 1);

    } else if(!strcmp(argv[1], "link") || !strcmp(argv[1], "autonomous-uart")){

        /* Simulates state and outputs supplied by the future race firmware. */
        state = RUNNING;
        drive(80, 80);
        uint8_t pwm = OCR0A;
        for(now = 10; now < 5000; now += 10){

            supervisor_step(&state);
            supervisor_after(&state);
            drain();

        }

        assert(state == RUNNING && OCR0A == pwm && OCR0B == pwm);
        for(uint8_t i = 0; i < 100; i++){

            UDR0 = 'x';
            USART_RX_vect();

        }

        supervisor_step(&state);
        supervisor_after(&state);
        assert(state == RUNNING && OCR0A == pwm && fault == 1);
        command(&state, 1, "MANUAL_DISABLE");
        assert(reply_code() == 3 && OCR0A == pwm);
        command(&state, 2, "ESTOP");
        assert(state == STOP && estop && !OCR0A);

    } else if(!strcmp(argv[1], "calibration") || !strcmp(argv[1], "local")){

        command(&state, 1, "CALIBRATE");
        drive(51, 51);
        now = 4000;
        supervisor_step(&state);
        supervisor_after(&state);
        assert(state == CALIBRATION && OCR0A);
        command(&state, 2, "MANUAL_DISABLE");
        assert(state == CALIBRATION && OCR0A && reply_code() == 3);
        state = WAITING;
        supervisor_after(&state);
        assert(calibrated);
        command(&state, 3, "START");
        assert(reply_code() == 6 && state == WAITING);

    } else if(!strcmp(argv[1], "duplicates")){

        command(&state, 1, "MANUAL_ENABLE");
        command(&state, 1, "CALIBRATE");
        assert(reply_code() == 5 && manual);
        command(&state, 1, "MANUAL_ENABLE");
        assert(reply_code() == 0);
        command(&state, 1, "ESTOP");
        assert(estop && !manual);

    } else if(!strcmp(argv[1], "pid")){

        command(&state, 1, "GET_PID");
        assert(strstr(wire, ",G,145,8,32*"));
        command(&state, 2, "SET_PID,1000,500,500");
        assert(reply_code() == 0 && gains.kp == 1000 && gains.ki == 500);
        assert(supervisor_get_gains()->kd == 500);
        command(&state, 3, "SET_PID,1001,0,0");
        assert(reply_code() == 2 && gains.kp == 1000);
        command(&state, 4, "SET_PID,100,0");
        assert(reply_code() == 2 && gains.kp == 1000);
        command(&state, 5, "MANUAL_ENABLE");
        command(&state, 6, "SET_PID,60,0,0");
        assert(reply_code() == 3);
        command(&state, 7, "ESTOP");
        command(&state, 8, "SET_PID,60,0,0");
        assert(reply_code() == 4);
        command(&state, 9, "REARM");
        command(&state, 10, "SET_PID,60,0,0");
        assert(reply_code() == 0 && gains.kp == 60);
        state = RUNNING;
        command(&state, 11, "SET_PID,100,0,0");
        assert(reply_code() == 3 && gains.kp == 60);

    } else if(!strcmp(argv[1], "congestion")){

        command(&state, 1, "PING");
        now = 200;
        supervisor_after(&state); /* Deliberately do not drain TX. */
        for(uint8_t i = 0; i < 20; i++)
            supervisor_after(&state);
        assert(((th - tt) & (TX_SIZE - 1)) < TX_SIZE);
        command(&state, 2, "GET_PID");
        assert(reply_code() == 0 && strstr(wire, ",G,145,8,32*"));

    } else if(!strcmp(argv[1], "wire")){

        command(&state, 255, "PING");
        fputs(wire, stdout);
        command(&state, 254, "GET_STATUS");
        fputs(wire, stdout);
        command(&state, 253, "GET_SENSORS");
        fputs(wire, stdout);
        command(&state, 252, "SET_PID,1000,500,500");
        fputs(wire, stdout);
        return 0;

    } else
        assert(0);
    printf("PASS basic communication %s\n", argv[1]);
    return 0;

}

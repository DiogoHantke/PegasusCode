/* Comunicação básica: sem controlador de corrida, heap ou printf no AVR. */
#include "adc_lib.h"
#include "calibration.h"
#include "pegasus_config.h"
#include "pegasus_protocol.h"
#include "pwm_lib.h"
#include "supervisor.h"
#include "timer_lib.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <util/atomic.h>
#ifndef BT_BAUD
#define BT_BAUD 9600UL
#endif
#define RX_SIZE 32U
#define TX_SIZE 64U
#define PERIOD_MS 200U
#define MONITOR_TIMEOUT_MS 1500U
#define MANUAL_TIMEOUT_MS 300U
#define NONE 255U
#define R_ID 1U
#define R_GAINS 2U
#define R_STATUS 4U
#define R_SENSORS 8U
#define R_DIAG 16U

static volatile uint8_t rx[RX_SIZE];
static volatile uint8_t tx[TX_SIZE];
static volatile uint8_t rh;
static volatile uint8_t rt;
static volatile uint8_t th;
static volatile uint8_t tt;
static volatile uint8_t overflow;
static char frame[FRAME_MAX];
static uint8_t used;
static uint8_t manual;
static uint8_t estop;
static uint8_t fault;
static uint8_t calibrated;
static uint8_t sample = 1;
static uint8_t replies;
static uint8_t ack_seq;
static uint8_t ack_code = NONE;
static uint8_t have_last;
static uint8_t last_seq;
static uint8_t last_crc;
static uint8_t last_result;
static uint8_t monitor;
static uint8_t diagnostic = 1;
static uint16_t last_move;
static uint16_t last_ping;
static uint16_t last_telemetry;
static STATE previous_state = INIT;
static SUPERVISOR_GAINS gains = {145, 8, 32};
/* O cursor TX so e publicado depois de concluir o frame e o CRC. */
static uint8_t cursor;
static uint8_t checksum;

ISR(USART_RX_vect){

    uint8_t status = UCSR0A;
    uint8_t c = UDR0;
    uint8_t next = (rh + 1U) & (RX_SIZE - 1U);
    if((status & ((1 << FE0) | (1 << DOR0) | (1 << UPE0))) || next == rt)
        overflow = 1;
    else{

        rx[rh] = c;
        rh = next;

    }

}

ISR(USART_UDRE_vect){

    if(tt == th)
        UCSR0B &= ~(1 << UDRIE0);
    else{

        UDR0 = tx[tt];
        tt = (tt + 1U) & (TX_SIZE - 1U);

    }

}

static void halt(void){

    pwm_set_duty(PWM_A, 0);
    pwm_set_duty(PWM_B, 0);

}

static void drive(int16_t l, int16_t r){

    halt();
    if(l >= 0){

        PORTB &= ~(1 << MOTOR_L_IN1);
        PORTB |= 1 << MOTOR_L_IN2;

    }else{

        PORTB |= 1 << MOTOR_L_IN1;
        PORTB &= ~(1 << MOTOR_L_IN2);

    }

    if(r >= 0){

        PORTD |= 1 << MOTOR_R_IN1;
        PORTD &= ~(1 << MOTOR_R_IN2);

    }else{

        PORTD &= ~(1 << MOTOR_R_IN1);
        PORTD |= 1 << MOTOR_R_IN2;

    }

    pwm_set_duty(PWM_B, (uint16_t)(l < 0 ? -l : l) * 100U / 255U);
    pwm_set_duty(PWM_A, (uint16_t)(r < 0 ? -r : r) * 100U / 255U);

}

static void raw(char c){

    tx[cursor] = c;
    cursor = (cursor + 1U) & (TX_SIZE - 1U);

}

static void byte(char c){

    checksum ^= (uint8_t)c;
    for(uint8_t i = 0; i < 8; i++)
        checksum = checksum & 128U ? (uint8_t)((checksum << 1) ^ 7U) : (uint8_t)(checksum << 1);
    raw(c);

}

static void decimal(uint16_t n){

    char digits[5];
    uint8_t count = 0;
    do {

        digits[count++] = '0' + n % 10U;
        n /= 10U;

    } while(n);
    while(count)
        byte(digits[--count]);

}

static void field(uint16_t n){

    byte(',');
    decimal(n);

}

static void signed_field(int16_t n){

    byte(',');
    if(n < 0){

        byte('-');
        n = -n;

    }

    decimal((uint16_t)n);

}

static void flash(PGM_P p){

    char c;
    while((c = pgm_read_byte(p++)))
        byte(c);

}

static uint8_t begin(uint8_t seq, PGM_P type, uint8_t required){

    if(((tt - th - 1U) & (TX_SIZE - 1U)) < required)
        return 0;
    cursor = th;
    checksum = 0;
    raw('@');
    decimal(seq);
    byte(',');
    flash(type);
    return 1;

}

static char hex(uint8_t n){

    return n < 10 ? '0' + n : 'A' + n - 10;

}

static void finish(void){

    raw('*');
    raw(hex(checksum >> 4));
    raw(hex(checksum & 15));
    raw('\n');
    th = cursor;
    UCSR0B |= 1 << UDRIE0;

}

/* A telemetria reserva 16 bytes para ACK/NACK. */
static void transmit(STATE state){

    if(ack_code != NONE){

        if(!begin(ack_seq, ack_code ? PSTR("NACK") : PSTR("ACK"), 15))
            return;
        field(ack_code);
        finish();
        ack_code = NONE;

    }

    if(replies & R_ID){

        if(!begin(0, PSTR("ID"), 48))
            return;
        byte(',');
        flash(PSTR("PEGASUS"));
        field(1);
        field(0);
        field(0);
        finish();
        replies &= ~R_ID;

    } else if(replies & R_GAINS){

        if(!begin(0, PSTR("G"), 40))
            return;
        field(gains.kp);
        field(gains.ki);
        field(gains.kd);
        finish();
        replies &= ~R_GAINS;

    } else if(replies & R_STATUS){

        if(!begin(0, PSTR("T"), 48))
            return;
        field(state);
        field(manual | (estop << 1) | (calibrated << 2));
        field(fault);
        signed_field((PORTB & (1 << MOTOR_L_IN2)) ? OCR0B : -(int16_t)OCR0B);
        signed_field((PORTD & (1 << MOTOR_R_IN1)) ? OCR0A : -(int16_t)OCR0A);
        field(0); /* Cronometragem pendente do firmware de corrida. */
        field(calibration_phase());
        field(CAL_WHITE_HIGH);
        finish();
        replies &= ~R_STATUS;

    } else if(replies & R_SENSORS){

        if(!begin(0, PSTR("S"), 52))
            return;
        for(uint8_t i = 1; i <= 6; i++)
            field(adc_latest(i));
        field(adc_valid_mask());
        finish();
        replies &= ~R_SENSORS;

    } else if(replies & R_DIAG){

        if(!begin(0, PSTR("D"), 40))
            return;
        uint8_t low;
        uint8_t high;
        uint8_t ok;
        calibration_get(diagnostic, &low, &high, &ok);
        field(diagnostic);
        field(low);
        field(high);
        field(ok);
        finish();
        replies &= ~R_DIAG;
        if(++diagnostic > 6)
            diagnostic = 1;

    }

}

static uint8_t number(const char *p, uint16_t *result){

    if(!p || !*p)
        return 0;
    uint16_t n = 0;
    do {

        if(*p < '0' || *p > '9' || n > 1000)
            return 0;
        n = n * 10U + (*p++ - '0');

    } while(*p);
    *result = n;
    return 1;

}

#define IS(text) (!strcmp_P(p, PSTR(text)))

static uint8_t execute(char *p, STATE *state, uint16_t now){

    if(!strncmp_P(p, PSTR("SET_PID,"), 8)){

        uint16_t values[3];
        char *part = p + 8;
        for(uint8_t i = 0; i < 3; i++){

            char *next = strchr(part, ',');
            if((i < 2 && !next) || (i == 2 && next))
                return 2;
            if(next)
                *next++ = 0;
            if(!number(part, &values[i]) || values[i] > (i ? 500 : 1000))
                return 2;
            part = next;

        }

        if(estop)
            return 4;
        if(manual || (*state != INIT && *state != WAITING && *state != STOP && *state != FINISH))
            return 3;
        gains.kp = values[0];
        gains.ki = values[1];
        gains.kd = values[2];
        replies |= R_GAINS;
        return 0;

    }

    if(IS("SET_PID"))
        return 2;
    char *arg = strchr(p, ',');
    uint16_t value = 0;
    if(arg){

        *arg++ = 0;
        if(!number(arg, &value))
            return 2;

    }

    if(IS("STOP") || IS("ESTOP")){

        if(arg)
            return 2;
        halt();
        manual = 0;
        if(IS("ESTOP"))
            estop = 1;
        if(*state == CALIBRATION)
            calibrated = 0;
        *state = STOP;
        return 0;

    }

    if(IS("PING")){

        if(arg)
            return 2;
        monitor = 1;
        last_ping = now;
        replies |= R_ID;
        return 0;

    }

    if(IS("GET_STATUS")){

        if(arg)
            return 2;
        replies |= R_STATUS;
        return 0;

    }

    if(IS("GET_SENSORS")){

        if(arg)
            return 2;
        replies |= R_SENSORS;
        return 0;

    }

    if(IS("GET_PID")){

        if(arg)
            return 2;
        replies |= R_GAINS;
        return 0;

    }

    if(IS("REARM")){

        if(arg || *state != STOP)
            return 3;
        halt();
        estop = manual = fault = 0;
        *state = WAITING;
        return 0;

    }

    if(estop)
        return 4;
    if(IS("START"))
        return arg ? 2 : 6; /* Corrida ainda nao implementada. */
    if(IS("MANUAL_STOP")){

        if(arg)
            return 2;
        if(!manual || *state != WAITING)
            return 3;
        halt();
        last_move = now;
        return 0;

    }

    if(IS("MANUAL_DISABLE")){

        if(arg)
            return 2;
        if(*state == RUNNING || *state == CALIBRATION)
            return 3;
        if(manual){

            halt();
            manual = 0;
            *state = WAITING;

        }

        return 0;

    }

    if(IS("CALIBRATE") || IS("MANUAL_ENABLE")){

        if(arg)
            return 2;
        if(*state == RUNNING || *state == CALIBRATION || *state == EMERGENCY)
            return 3;
        halt();
        fault = 0;
        if(IS("CALIBRATE")){

            manual = calibrated = 0;
            calibration_reset();
            *state = CALIBRATION;

        }else{

            manual = 1;
            *state = WAITING;
            last_move = now;
            PORTB |= 1 << SENSOR_ON;

        }

        return 0;

    }

    if(!strncmp_P(p, PSTR("MANUAL_"), 7)){

        if(!manual || *state != WAITING)
            return 3;
        if(!arg || value > 160)
            return 2;
        if(IS("MANUAL_FORWARD"))
            drive(value, value);
        else if(IS("MANUAL_BACKWARD"))
            drive(-value, -value);
        else if(IS("MANUAL_LEFT"))
            drive(-value, value);
        else if(IS("MANUAL_RIGHT"))
            drive(value, -value);
        else
            return 1;
        last_move = now;
        return 0;

    }

    return 1;

}

void supervisor_init(void){

    uint16_t divisor = F_CPU / 16UL / BT_BAUD - 1;
    UBRR0H = divisor >> 8;
    UBRR0L = divisor;
    UCSR0A = 0;
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    halt();
    PORTB |= 1 << SENSOR_ON;

}

void supervisor_step(STATE *state){

    uint16_t now = timer1_elapsed_milliseconds();
    if(manual && *state != WAITING)
        manual = 0; /* Preserva o estado definido pelo firmware. */
    if(overflow){

        ATOMIC_BLOCK(ATOMIC_RESTORESTATE){

            rt = rh;
            overflow = 0;

        }

        used = 0;
        fault = 1;
        if(manual){

            halt();
            manual = 0;
            *state = STOP;

        }

    }

    transmit(*state);
    if(ack_code == NONE){

        uint8_t budget = RX_SIZE;
        while(rt != rh && budget--){

            char c = rx[rt];
            rt = (rt + 1U) & (RX_SIZE - 1U);
            if(c == '@')
                used = 0;
            if(!used && c != '@')
                continue;
            if(used >= FRAME_MAX - 1U){

                used = 0;
                continue;

            }

            frame[used++] = c;
            if(c != '\n')
                continue;
            frame[used] = 0;
            used = 0;
            uint8_t seq;
            char *p;
            if(!protocol_decode(frame, &seq, &p))
                continue;
            uint8_t crc = protocol_crc(p, strlen(p));
            uint8_t special = IS("STOP") || IS("ESTOP") || IS("PING") || IS("GET_STATUS") ||
                              IS("GET_SENSORS") || IS("GET_PID");
            if(!special && have_last && seq == last_seq)
                ack_code = crc == last_crc ? last_result : 5;
            else{

                ack_code = execute(p, state, now);
                if(!special){

                    have_last = 1;
                    last_seq = seq;
                    last_crc = crc;
                    last_result = ack_code;

                }

            }

            ack_seq = seq;
            break;

        }

    }

    if(manual && (uint16_t)(now - last_move) > MANUAL_TIMEOUT_MS){

        halt();
        manual = 0;
        fault = 2;
        *state = STOP;

    }

    if(monitor && (uint16_t)(now - last_ping) > MONITOR_TIMEOUT_MS){

        monitor = 0;
        replies &= R_ID | R_GAINS;

    }

    transmit(*state);

}

void supervisor_after(STATE *state){

    if(previous_state == CALIBRATION && *state == WAITING)
        calibrated = 1;
    if(*state == CALIBRATION || *state == EMERGENCY)
        calibrated = 0;
    if(*state != CALIBRATION && *state != RUNNING && *state != EMERGENCY){

        uint8_t value;
        if(adc_read(sample, &value) && ++sample > 6)
            sample = 1;

    }

    uint16_t now = timer1_elapsed_milliseconds();
    if(monitor && (uint16_t)(now - last_telemetry) >= PERIOD_MS){

        last_telemetry = now;
        replies |= R_STATUS | R_SENSORS | R_DIAG;

    }

    transmit(*state);
    previous_state = *state;

}

uint8_t supervisor_button_allowed(void){

    return !manual && !estop;

}

uint8_t supervisor_emergency_active(void){

    return estop;

}

const SUPERVISOR_GAINS *supervisor_get_gains(void){

    return &gains;

}

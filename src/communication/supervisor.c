#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "supervisor.h"
#include "pegasus_protocol.h"
#include "pegasus_config.h"
#include "calibration.h"
#include "adc_lib.h"
#include "pwm_lib.h"
#include "timer_lib.h"
#ifndef BT_BAUD
#define BT_BAUD 9600UL
#endif
#ifndef MANUAL_TIMEOUT_MS
#define MANUAL_TIMEOUT_MS 300U
#endif
#ifndef LINK_TIMEOUT_MS
#define LINK_TIMEOUT_MS 1500U
#endif
static volatile uint8_t rx[128], tx[256], rh, rt, th, tt, overflow;
static char frame[FRAME_MAX];
static uint8_t used, manual, estop, fault, calibrated, sample=1;
static uint8_t cache_seq[8], cache_crc[8], cache_result[8], cache_count, cache_pos;
static uint16_t last_link, last_move, last_telemetry, previous_ms, telemetry_ms=BT_BAUD<19200?200:100;
static uint32_t uptime, race_start, elapsed;
static int16_t left_pwm, right_pwm;
static STATE previous_state=INIT;
ISR(USART_RX_vect){
    uint8_t status=UCSR0A, c=UDR0, next=(rh+1)&127;
    if((status&((1<<FE0)|(1<<DOR0)|(1<<UPE0))) || next==rt)overflow=1;
    else {rx[rh]=c; rh=next;}
}
ISR(USART_UDRE_vect){
    if(tt==th)UCSR0B &= ~(1<<UDRIE0);
    else {UDR0=tx[tt]; tt++;}
}
static void halt(void){
    pwm_set_duty(PWM_A,0); pwm_set_duty(PWM_B,0);
    left_pwm=right_pwm=0;
}
static void drive(int16_t l,int16_t r){
    /* Preserve established calibration forward polarity. Stop PWM before reversing. */
    halt();
    if(l>=0){PORTB &= ~(1<<MOTOR_L_IN1); PORTB |= 1<<MOTOR_L_IN2;}
    else {PORTB |= 1<<MOTOR_L_IN1; PORTB &= ~(1<<MOTOR_L_IN2);}
    if(r>=0){PORTD |= 1<<MOTOR_R_IN1; PORTD &= ~(1<<MOTOR_R_IN2);}
    else {PORTD &= ~(1<<MOTOR_R_IN1); PORTD |= 1<<MOTOR_R_IN2;}
    left_pwm=l; right_pwm=r;
    pwm_set_duty(PWM_B,(uint16_t)abs(l)*100/255);
    pwm_set_duty(PWM_A,(uint16_t)abs(r)*100/255);
}
/* Whole frame enqueue; reserve 72 bytes for ACK. Never wait for UART. */
static uint8_t send(uint8_t seq,const char *body,uint8_t essential){
    char out[FRAME_MAX];
    int n=snprintf(out,sizeof(out),"@%u,%s",seq,body);
    if(n<0 || n>FRAME_MAX-5)return 0;
    uint8_t crc=protocol_crc(out+1,n-1);
    n+=snprintf(out+n,sizeof(out)-n,"*%02X\n",crc);
    uint8_t free=(uint8_t)(tt-th-1);
    if(n>free || (!essential && n+72>free))return 0;
    for(int i=0;i<n;i++){tx[th]=out[i]; th++;}
    UCSR0B |= 1<<UDRIE0;
    return 1;
}
static void ack(uint8_t seq,uint8_t error){
    char body[24]; snprintf(body,sizeof(body),"%s,%u",error?"NACK":"ACK",error);
    send(seq,body,1);
}
static uint8_t number(const char *s,uint16_t *n){
    if(!s || !*s)return 0;
    uint16_t v=0;
    while(*s){if(*s<'0'||*s>'9'||v>1000)return 0; v=v*10+(*s++-'0');}
    *n=v; return 1;
}
static void status(STATE state){
    char b[100];
    snprintf(b,sizeof(b),"T,%u,%u,%u,%d,%d,%lu,%u,%u",state,manual|(estop<<1)|(calibrated<<2),fault,left_pwm,right_pwm,(unsigned long)elapsed,calibration_phase(),CAL_WHITE_HIGH);
    send(0,b,0);
}
static void sensors(void){
    char b[72];
    snprintf(b,sizeof(b),"S,%u,%u,%u,%u,%u,%u,%u",adc_latest(1),adc_latest(2),adc_latest(3),adc_latest(4),adc_latest(5),adc_latest(6),adc_valid_mask());
    send(0,b,0);
}
static uint8_t execute(char *p,STATE *s,uint16_t now){
    char *arg=strchr(p,','); uint16_t value=0;
    if(arg){*arg++=0; if(!number(arg,&value))return 2;}
    if(!strcmp(p,"ESTOP") || !strcmp(p,"STOP")){
        if(arg)return 2;
        halt(); manual=0;
        if(!strcmp(p,"ESTOP"))estop=1;
        if(*s==CALIBRATION)calibrated=0;
        *s=STOP; return 0;
    }
    if(!strcmp(p,"PING")){
        if(arg)return 2;
        send(0,"ID,PEGASUS,1,0",1); return 0;
    }
    if(!strcmp(p,"GET_STATUS")){if(arg)return 2;status(*s);return 0;}
    if(!strcmp(p,"GET_SENSORS")){if(arg)return 2;sensors();return 0;}
    if(!strcmp(p,"TELEM")){
        if(!arg || value<(BT_BAUD<19200?200:50) || value>1000)return 2;
        telemetry_ms=value; return 0;
    }
    if(!strcmp(p,"REARM")){
        if(arg || *s!=STOP)return 3;
        halt(); estop=0; fault=0; manual=0; *s=WAITING; return 0;
    }
    if(estop)return 4;
    if(!strcmp(p,"MANUAL_STOP")){
        if(arg)return 2;
        if(!manual)return 3;
        halt();return 0;
    }
    if(!strcmp(p,"MANUAL_DISABLE")){
        if(arg)return 2;
        halt();manual=0; if(*s!=CALIBRATION && *s!=RUNNING)*s=WAITING;return 0;
    }
    if(!strcmp(p,"START")){
        if(arg)return 2;
        if(manual || !calibrated || (*s!=WAITING && *s!=STOP))return 3;
        fault=0;elapsed=0;race_start=uptime;*s=RUNNING;return 0;
    }
    if(!strcmp(p,"CALIBRATE") || !strcmp(p,"MANUAL_ENABLE")){
        if(arg)return 2;
        if(*s==RUNNING || *s==CALIBRATION || *s==EMERGENCY)return 3;
        halt();fault=0;
        if(!strcmp(p,"CALIBRATE")){manual=0;calibrated=0;calibration_reset();*s=CALIBRATION;}
        else {manual=1;*s=WAITING;last_move=now;PORTB |= 1<<SENSOR_ON;}
        return 0;
    }
    if(!strncmp(p,"MANUAL_",7)){
        if(!manual || *s!=WAITING)return 3;
        if(!arg || value>160)return 2;
        if(!strcmp(p,"MANUAL_FORWARD"))drive(value,value);
        else if(!strcmp(p,"MANUAL_BACKWARD"))drive(-value,-value);
        else if(!strcmp(p,"MANUAL_LEFT"))drive(-value,value);
        else if(!strcmp(p,"MANUAL_RIGHT"))drive(value,-value);
        else return 1;
        last_move=now;return 0;
    }
    return 1;
}
void supervisor_init(void){
    uint16_t ubrr=F_CPU/16UL/BT_BAUD-1;
    UBRR0H=ubrr>>8; UBRR0L=ubrr;
    UCSR0A=0;UCSR0C=(1<<UCSZ01)|(1<<UCSZ00);
    UCSR0B=(1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0);
    halt();PORTB |= 1<<SENSOR_ON;
}
void supervisor_step(STATE *s){
    timer1_start();
    uint16_t now=timer1_elapsed_milliseconds();
    uptime+=(uint16_t)(now-previous_ms);previous_ms=now;
    if(overflow){
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE){rt=rh;overflow=0;}
        used=0; halt();manual=0;fault=1;*s=STOP;
    }
    uint8_t budget=127;
    while(rt!=rh && budget--){
        char c=rx[rt];rt=(rt+1)&127;
        if(c=='@')used=0;
        if(!used && c!='@')continue;
        if(used>=FRAME_MAX-1){used=0;continue;}
        frame[used++]=c;
        if(c!='\n')continue;
        frame[used]=0;used=0;
        uint8_t seq;char *payload;
        if(!protocol_decode(frame,&seq,&payload))continue;
        uint8_t crc=protocol_crc(payload,strlen(payload)), found=0;
        for(uint8_t i=0;i<cache_count;i++)if(cache_seq[i]==seq){
            ack(seq,cache_crc[i]==crc?cache_result[i]:5);found=1;break;
        }
        if(found)continue; /* Duplicate cannot refresh movement lease or restart calibration. */
        uint8_t result=execute(payload,s,now);
        if(!result)last_link=now;
        cache_seq[cache_pos]=seq;cache_crc[cache_pos]=crc;cache_result[cache_pos]=result;
        cache_pos=(cache_pos+1)&7;if(cache_count<8)cache_count++;
        ack(seq,result);
    }
    if(manual && (uint16_t)(now-last_move)>MANUAL_TIMEOUT_MS){halt();manual=0;fault=2;*s=STOP;}
    if((*s==RUNNING || *s==CALIBRATION || manual) && (uint16_t)(now-last_link)>LINK_TIMEOUT_MS){
        halt();manual=0;fault=3;if(*s==CALIBRATION)calibrated=0;*s=STOP;
    }
}
static void follow_line(STATE *s){
    uint16_t total=0;int32_t weighted=0;
    for(uint8_t i=1;i<=6;i++){
        uint8_t low,high,ok;calibration_get(i,&low,&high,&ok);
        if(!ok || high<=low){halt();fault=4;*s=STOP;return;}
        int16_t v=((int16_t)adc_latest(i)-low)*255/(high-low);
        if(v<0)v=0;if(v>255)v=255;
        uint16_t black=CAL_WHITE_HIGH?255-v:v;
        total+=black;weighted+=(int32_t)black*((int16_t)i*2-7);
    }
    if(total<100){halt();fault=5;*s=STOP;return;}
    int16_t correction=weighted*12/total;
    int16_t l=80+correction,r=80-correction;
    if(l<0)l=0;if(l>140)l=140;
    if(r<0)r=0;if(r>140)r=140;
    drive(l,r);
}
void supervisor_after(STATE *s){
    uint16_t now=timer1_elapsed_milliseconds();
    if(previous_state==CALIBRATION && *s==WAITING)calibrated=1;
    if(*s==EMERGENCY){halt();fault=4;calibrated=0;}
    if(*s==CALIBRATION){calibrated=0;left_pwm=(uint16_t)OCR0B;right_pwm=(uint16_t)OCR0A;}
    else if(*s!=EMERGENCY){
        uint8_t value;
        if(adc_read(sample,&value)){
            if(++sample>6){sample=1;if(*s==RUNNING)follow_line(s);}
        }
    }
    if(*s==RUNNING)elapsed=uptime-race_start;
    if(*s!=RUNNING && !manual && *s!=CALIBRATION)halt();
    if((uint16_t)(now-last_telemetry)>=telemetry_ms){
        static uint8_t diagnostic=1;
        last_telemetry=now;status(*s);sensors();
        uint8_t low,high,ok;calibration_get(diagnostic,&low,&high,&ok);
        char b[40];snprintf(b,sizeof(b),"D,%u,%u,%u,%u",diagnostic,low,high,ok);
        send(0,b,0);if(++diagnostic>6)diagnostic=1;
    }
    previous_state=*s;
}

/* Host test: real application, ADC, PWM, USART and normalization; fake hardware. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define main firmware_main
#include "../../src/main.c"
#undef main

uint8_t DDRD, DDRB, PORTD, PORTB, PIND;
uint8_t ADMUX, ADCSRA, ADCH;
uint8_t TCCR0A, TCCR0B, TCNT0, OCR0A, OCR0B;
uint8_t UBRR0H, UBRR0L, UCSR0A, UCSR0B, UCSR0C;
static uint16_t seconds;
static uint8_t timer_running, serial_only, final_samples;
static uint8_t data_register, pending;
static char output[1024];
static unsigned output_size;

static void flush_output(void){
    if (pending){
        assert(output_size + 1 < sizeof output);
        output[output_size++] = (char)data_register;
        output[output_size] = '\0';
        pending = 0;
    }
}

uint8_t *test_udr(void){
    if (!serial_only){
        assert(OCR0A == 0 && OCR0B == 0);
        assert(final_samples == 6);
        assert(!timer_running);
    }
    flush_output();
    pending = 1;
    return &data_register;
}

void timer1_ctc_init(void){}
void timer1_interrupt_enable(void){}
void timer1_start(void){ timer_running = 1; }
void timer1_stop(void){ timer_running = 0; }
uint16_t timer1_elapsed_seconds(void){ return seconds; }

int main(int argc, char **argv){
    assert(argc == 2);
    /* Exhaust every 8-bit combination, including equal and reversed bounds. */
    for (unsigned min = 0; min <= 255; min++){
        for (unsigned max = 0; max <= 255; max++){
            for (unsigned value = 0; value <= 255; value++){
                unsigned expected = 0;
                if (max > min && value > min){
                    expected = value >= max ? 1000 : (value-min)*1000/(max-min);
                }
                assert(sensor_normalize(value, min, max) == expected);
            }
        }
    }
    assert(sensor_normalize(125, 30, 220) == 500);
    assert(sensor_normalize(129, 32, 225) == 502);

    usart_init();
    assert(UBRR0H == 0 && UBRR0L == 16);
    assert(UCSR0A == (1 << U2X0));
    assert(UCSR0B == (1 << TXEN0));
    assert(UCSR0C == ((1 << UCSZ01) | (1 << UCSZ00)));
    assert((DDRD & 3) == 2);
    UCSR0A |= (1 << UDRE0); /* Simulate ready transmit hardware. */

    if (!strcmp(argv[1], "serial")){
        serial_only = 1;
        usart_transmit_uint(0);
        usart_transmit_char(',');
        usart_transmit_uint(65535);
        usart_transmit_string("\r\n");
        flush_output();
        assert(!strcmp(output, "0,65535\r\n"));
        puts("PASS normalization (16777216 combinations), USART config and integers");
        return 0;
    }

    STATE state = CALIBRATION;
    PIND = (1 << BUTTON);
    adc_init();
    pwm_init();
    calibration_sensor(&state);
    assert(OCR0A && OCR0B && timer_running);

    if (!strcmp(argv[1], "timeout")){
        seconds = 11;
        calibration_sensor(&state);
        assert(state == CALIBRATION && OCR0A && OCR0B);
        seconds = 12;
        calibration_sensor(&state);
        assert(state == EMERGENCY && !OCR0A && !OCR0B);
        assert(!timer_running && !(PORTB & (1 << SENSOR_ON)));
        for (unsigned i = 0; i < 1000; i++)calibration_sensor(&state);
        assert(!output_size && !pending);
        puts("PASS timeout at 12 s, motors stopped, no success report");
        return 0;
    }

    unsigned samples[8] = {0};
    unsigned conversion_ticks = 0;
    for (unsigned i = 0; i < 10000; i++){
        if (ADCSRA & (1 << ADSC)){
            /* Keep conversion pending for several polls, with stale ADCH. */
            if (++conversion_ticks == 3){
                uint8_t channel = ADMUX & 15;
                assert(channel >= 1 && channel <= 6);
                if (!OCR0A && !OCR0B){
                    assert(channel == ++final_samples);
                    assert(PORTB & (1 << SENSOR_ON));
                    ADCH = 124 + channel;
                }else{
                    ADCH = samples[channel]++ < 10 ? 30 : 220;
                }
                ADCSRA &= ~(1 << ADSC);
                conversion_ticks = 0;
            }
        }
        calibration_sensor(&state);
    }
    flush_output();
    char expected[1024] = "CAL,OK";
    for (unsigned channel = 1; channel <= 6; channel++){
        char field[80];
        snprintf(field, sizeof field, ",QTR0%u:MIN=30:MAX=220:RAW=%u:NORM=%u",
                 channel, 124+channel, (94+channel)*1000/190);
        strcat(expected, field);
    }
    strcat(expected, "\r\n");
    assert(!strcmp(output, expected));
    assert(state == CALIBRATION); /* Lateral calibration is still pending. */
    assert(final_samples == 6);
    puts("PASS final ADC acquisition, stopped motors, exact report, no repeats in 10000 polls");
    fputs(output, stdout);
    return 0;
}

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <stdint.h>

#include "adc_lib.h"
#include "timer_lib.h"

#define SENSORS_QUANTITY 8

#define BUTTON PD2

#define RX PD0
#define TX PD1

#define QTR00 0U
#define QTR01 1U
#define QTR02 2U
#define QTR03 3U
#define QTR04 4U
#define QTR05 5U
#define QTR06 6U
#define QTR07 7U


typedef enum {
    CALIBRATION,
    WAITING,
    RUNNING,
    FINISH,
    STOP
}STATE;

void calibration_sensor(STATE *state);

int main(void){

    STATE state = CALIBRATION;

    DDRD &= ~(1 << BUTTON);
    PORTD |= (1 << BUTTON);

    adc_init();
    timer1_ctc_init();

    for (;;){
        switch (state)
        {
        case CALIBRATION:
            calibration_sensor(&state);
            break;

        case WAITING:
            break;

        case RUNNING:
            break;

        case FINISH:
            break;

        case STOP:
            break;

        default:
            break;
        }
    }

    return 0;
}

void calibration_sensor(STATE *state) {

    static uint8_t sensors_min[SENSORS_QUANTITY];
    static uint8_t sensors_max[SENSORS_QUANTITY];

    static uint8_t sensors_indentifier[SENSORS_QUANTITY] = {QTR00, QTR01 ,QTR02, QTR03, QTR04, QTR05, QTR06, QTR07};

    volatile uint8_t button_state = PORTB & (1 << BUTTON);
    static uint8_t calibration_state = 0;

    if (!button_state) {

        timer1_start();

        calibration_state = 1;

        for (uint8_t index = 0; index < SENSORS_QUANTITY; index++) {
            sensors_min[index] = 255;
            sensors_max[index] = 0;
        }



        timer1_stop();
        *state = WAITING;
        calibration_state = 0;
    }
}

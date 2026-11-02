#include <avr/io.h>
#include "adc_lib.h"


void adc_init(void){
    ADMUX = (1 << REFS0) | (1 << ADLAR);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint8_t adc_read(uint8_t channel, uint8_t *value){
    static uint8_t converting = 0;
    static uint8_t active_channel = 0x00;

    if (channel > 7 || value == 0)return 0;

    if (converting){
        if (ADCSRA & (1 << ADSC))return 0;

        uint8_t result = ADCH;
        converting = 0;

        if (active_channel == channel){
            *value = result;
            return 1;
        }
        
    }else{
        ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
        active_channel = channel;
        ADCSRA |= (1 << ADSC);
        converting = 1;

    }

    return 0;
}


#include <avr/io.h>

#include "adc_lib.h"

static uint8_t latest[8];
static uint8_t valid;

uint8_t adc_latest(uint8_t channel){

    if(channel >= 8)return 0;

    return latest[channel];

}

uint8_t adc_valid_mask(void){

    return valid;

}

void adc_init(void){

    ADMUX = (1 << REFS0) | (1 << ADLAR);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

}

uint8_t adc_read_10(uint8_t channel, uint16_t *value){

    static uint8_t converting = 0;
    static uint8_t active_channel = 0x00;

    if (channel > 7 || value == 0)return 0;

    if (converting){

        if (ADCSRA & (1 << ADSC))return 0;

        uint8_t low = ADCL;
        uint8_t high = ADCH;
        uint16_t result = ((uint16_t)high << 2) | (low >> 6);
        converting = 0;

        if (active_channel == channel){

            latest[channel] = result >> 2;
            valid |= 1U << channel;
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

uint8_t adc_read(uint8_t channel, uint8_t *value){

    uint16_t raw;
    if(!value || !adc_read_10(channel, &raw))return 0;
    *value = raw >> 2;
    return 1;

}

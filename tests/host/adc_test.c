#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <avr/io.h>
#include "adc_lib.h"

uint8_t ADMUX, ADCSRA, ADCL, ADCH;
static void converted(uint16_t value){
    ADCL = (value & 3U) << 6;
    ADCH = value >> 2;
    ADCSRA &= ~(1 << ADSC);
}
int main(void){
    adc_init();
    assert(ADMUX == ((1 << REFS0) | (1 << ADLAR)));
    uint16_t raw;
    uint8_t reduced;
#ifdef PEGASUS_SENSOR_MONITOR
    assert(!adc_monitor_valid_mask() && !adc_monitor_latest(8));
#endif
    assert(!adc_read_10(8,&raw) && !adc_read_10(0,0) && !adc_read(0,0));
    for(unsigned value = 0; value <= 1023; value++){
        assert(!adc_read_10(6,&raw));
        assert((ADMUX & 15) == 6 && (ADCSRA & (1 << ADSC)));
        assert(!adc_read_10(6,&raw));
        converted(value);
        assert(adc_read_10(6,&raw) && raw == value);
        assert(!adc_read(1,&reduced));
        converted(value);
        assert(adc_read(1,&reduced) && reduced == value / 4);
    }
    assert(!adc_read(7,&reduced));
    converted(1023);
    assert(!adc_read_10(0,&raw)); /* Conversao pendente de outro canal descartada. */
    assert(!adc_read_10(0,&raw));
    converted(30);
    assert(adc_read_10(0,&raw) && raw == 30);
#ifdef PEGASUS_SENSOR_MONITOR
    assert(adc_monitor_latest(0) == 30);
    assert(adc_monitor_latest(1) == 1023 && adc_monitor_latest(6) == 1023);
    assert(adc_monitor_valid_mask() == ((1 << 0) | (1 << 1) | (1 << 6)));
#endif
    puts("ADC 10-bit acquisition and 8-bit calibration: OK");
}

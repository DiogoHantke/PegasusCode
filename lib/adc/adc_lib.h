#ifndef ADC_LIB_H
#define ADC_LIB_H

#include <stdint.h>

void adc_init(void);
uint8_t adc_read(uint8_t channel, uint8_t *value);

#endif

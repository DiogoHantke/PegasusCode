#ifndef SENSOR_LIB_H
#define SENSOR_LIB_H

#include <stdint.h>

/* ADC de 8 bits; escala crescente 0..1000. Intervalo invalido retorna 0. */
uint16_t sensor_normalize(uint8_t value, uint8_t min, uint8_t max);

#endif

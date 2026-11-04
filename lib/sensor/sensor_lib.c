#include "sensor_lib.h"

uint16_t sensor_normalize(uint8_t value, uint8_t min, uint8_t max){

    if (max <= min)return 0;
    if (value <= min)return 0;
    if (value >= max)return 1000;

    return (uint16_t)(((uint32_t)(value - min) * 1000UL) / (max - min));
}

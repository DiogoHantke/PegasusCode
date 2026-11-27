#ifndef PEGASUS_CALIBRATION_H
#define PEGASUS_CALIBRATION_H

#include "pegasus_state.h"
#include <stdint.h>

void calibration_sensor(STATE *state);
void calibration_reset(void);

void calibration_get(uint8_t channel, uint8_t *low, uint8_t *high, uint8_t *approved);
uint8_t calibration_phase(void);
uint8_t calibration_front_mask(void);
#endif

#ifndef PEGASUS_CALIBRATION_INTERNAL_H
#define PEGASUS_CALIBRATION_INTERNAL_H
#include <stdint.h>
#include "calibration.h"
#include "pegasus_config.h"

typedef struct {
    uint8_t channel, min, max;
    uint16_t value_mean;
    uint8_t count_mean, candidate_mean, mean_initialized, transition_pending;
    uint8_t contrast, approved;
} SENSOR;

void sensor_calibration_update(SENSOR *sensor, uint8_t value_adc);
uint8_t diagnostic_sensor_approved(SENSOR *sensor);
#endif

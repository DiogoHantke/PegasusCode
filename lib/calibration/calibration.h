#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "pegasus_state.h"
#include "sensor_lib.h"
#include "flash_lib.h"

typedef enum {
    INIT_CALIBRATION,
    FRONT_CALIBRATION,
    LATERAL_CALIBRATION,
    DIAGNOSTIC,
    FRONT_REPOSITION,
    LATERAL_REPOSITION,
    CALIBRATION_FAULT,
    CALIBRATION_SETTLE,
    CALIBRATION_READY,
    FRONT_POSITION
} STATE_CALIBRATION;

typedef enum {
    CAL_FAULT_NONE,
    CAL_FAULT_ATTEMPTS,
    CAL_FAULT_REFERENCE,
    CAL_FAULT_MOVEMENT,
    CAL_FAULT_INTERRUPTED,
    CAL_FAULT_TIMING
} CALIBRATION_CAUSE;

typedef struct {

    STATE_CALIBRATION phase;
    CALIBRATION_CAUSE cause;

    uint8_t failed_sensors;
    uint8_t frontal_retries;
    uint8_t lateral_retries;
    uint8_t valid;
    uint8_t restored;
    flash_status_t persistence;

} CALIBRATION_STATUS;

void calibration_reset(void);
void calibration_init_update(STATE *state);
void calibration_abort(STATE *state);
uint8_t calibration_is_valid(void);
void calibration_sensor(STATE *state);
const SENSOR *calibration_sensor_get(uint8_t channel);
CALIBRATION_STATUS calibration_status(void);

#endif

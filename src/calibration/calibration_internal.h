#ifndef PEGASUS_CALIBRATION_INTERNAL_H
#define PEGASUS_CALIBRATION_INTERNAL_H

/* Private data shared only by the calibration modules. */

#include <stdint.h>

#include "calibration.h"
#include "pegasus_config.h"

typedef enum {

    INIT_CALIBRATION,
    INIT_FRONT_CALIBRATION,
    FRONT_CALIBRATION,
    INIT_LATERAL_CALIBRATION,
    LATERAL_CALIBRATION,
    DIAGNOSTIC,
    CALIBRATION_FAULT

} STATE_CALIBRATION;

typedef enum {

    DIAGNOSTIC_CHECK,
    DIAGNOSTIC_FRONT_RECALIBRATION,
    DIAGNOSTIC_LATERAL_RECALIBRATION,
    DIAGNOSTIC_COMPLETE

} STATE_DIAGNOSTIC;

typedef struct {

    uint8_t channel;

    uint8_t min;
    uint8_t max;

    uint16_t value_mean;
    uint8_t count_mean;

    uint8_t candidate_mean;

    uint8_t mean_initialized;
    uint8_t transition_pending;

    uint8_t contrast;
    uint8_t approved;

} SENSOR;

typedef struct {

    SENSOR sensors[SENSORS_QUANTITY];
    uint8_t index;
    uint8_t recalibration_initialized;
    uint8_t recalibration_count;
    uint8_t recalibration_mask;
    uint8_t reference_white_count[SENSORS_QUANTITY];
    uint8_t reference_black_count[SENSORS_QUANTITY];
    uint8_t white_seen;
    uint8_t initial_crossed_line;
    uint16_t calibration_start_seconds;
    uint16_t lateral_stop_seconds;
    STATE_CALIBRATION state_calibration;
    STATE_DIAGNOSTIC state_diagnostic;

} CALIBRATION_CONTEXT;

void calibration_prepare(CALIBRATION_CONTEXT *context);
void calibration_front_start(CALIBRATION_CONTEXT *context);
void calibration_front_step(CALIBRATION_CONTEXT *context);
void calibration_lateral_start(CALIBRATION_CONTEXT *context);
void calibration_lateral_step(CALIBRATION_CONTEXT *context);
void calibration_diagnostic_step(CALIBRATION_CONTEXT *context, STATE *state);

void sensor_calibration_update(SENSOR *sensor, uint8_t value_adc);
uint8_t diagnostic_sensor_approved(SENSOR *sensor);
uint8_t diagnostic_recalibration_update(SENSOR *sensors, uint8_t recalibration_mask, uint8_t front, uint8_t *index, uint8_t *white_count);
uint8_t calibration_line_update(uint8_t channel, uint8_t value_adc, uint8_t *white_seen, uint8_t *white_count, uint8_t *black_count);

#endif

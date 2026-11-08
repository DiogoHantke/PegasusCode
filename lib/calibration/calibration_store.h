#ifndef CALIBRATION_STORE_H
#define CALIBRATION_STORE_H

#include "sensor_lib.h"
#include "flash_lib.h"

uint8_t calibration_store_read(SENSOR *frontal, SENSOR *lateral);
flash_status_t calibration_store_write(const SENSOR *frontal, const SENSOR *lateral);

#endif

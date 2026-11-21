/* Filtro por canal, extremos aceitos e aprovação por contraste. */

#include "calibration_internal.h"

void sensor_calibration_update(SENSOR *sensor, uint8_t value_adc){

    uint8_t absolute_mean = 0;
    uint8_t difference = 0;

    sensor->value_mean += value_adc;

    if (++sensor->count_mean >= CAL_SAMPLES){

        absolute_mean = (uint8_t)(sensor->value_mean / sensor->count_mean);

        sensor->count_mean = 0;
        sensor->value_mean = 0;

        if(!sensor->mean_initialized){

            sensor->mean_initialized = 1;
            sensor->candidate_mean = absolute_mean;

        }else{

            difference = (sensor->candidate_mean > absolute_mean) ? (sensor->candidate_mean - absolute_mean) : (absolute_mean - sensor->candidate_mean);

            if(difference < THRESHOLD_LINE && !sensor->transition_pending){

                if(sensor->max < sensor->candidate_mean)sensor->max = sensor->candidate_mean;

                if(sensor->min > sensor->candidate_mean)sensor->min = sensor->candidate_mean;

            }else if(difference >= THRESHOLD_LINE && !sensor->transition_pending){

                sensor->transition_pending = 1;

            }else if(sensor->transition_pending){

                if(difference < CAL_CONFIRM_TOLERANCE){

                    if(sensor->max < sensor->candidate_mean)sensor->max = sensor->candidate_mean;

                    if(sensor->min > sensor->candidate_mean)sensor->min = sensor->candidate_mean;

                    sensor->transition_pending = 0;
                }
            }

            sensor->candidate_mean = absolute_mean;
        }
    }

}

uint8_t diagnostic_sensor_approved(SENSOR *sensor){

    sensor->contrast = sensor->min <= sensor->max ? sensor->max - sensor->min : 0;
    sensor->approved = sensor->contrast >= CAL_MIN_CONTRAST;

    return sensor->approved;

}

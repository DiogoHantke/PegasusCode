/* Confirmação de branco/preto e aquisição durante a recalibração. */

#include "calibration_internal.h"
#include "adc_lib.h"

uint8_t calibration_line_update(uint8_t channel, uint8_t value_adc, uint8_t *white_seen, uint8_t *white_count, uint8_t *black_count){

    uint8_t white = CAL_WHITE_HIGH ? value_adc > CAL_LINE_THRESHOLD : value_adc < CAL_LINE_THRESHOLD;

    if(white){

        black_count[channel] = 0;

        if(white_count[channel] < CAL_SAMPLES)white_count[channel]++;

        if(white_count[channel] >= CAL_SAMPLES)*white_seen |= (1U << channel);
    }else{

        white_count[channel] = 0;

        if(*white_seen & (1U << channel)){

            if(++black_count[channel] >= CAL_SAMPLES * 2U)return 1;
        }
    }

    return 0;

}

uint8_t diagnostic_recalibration_update(SENSOR *sensors, uint8_t recalibration_mask, uint8_t front, uint8_t *index, uint8_t *white_count){

    SENSOR *sensor = &sensors[*index];
    uint8_t lateral = sensor->channel == QTR00_L || sensor->channel == QTR07_L;
    uint8_t reference = front ? lateral : !lateral;
    uint8_t value_adc = 0;
    uint8_t limit = 0;

    if(reference && sensor->approved){

        if(!adc_read(sensor->channel, &value_adc))return 0;

        uint8_t threshold = sensor->min + (sensor->max - sensor->min) / 2;
        uint8_t white = CAL_WHITE_HIGH ? value_adc > threshold : value_adc < threshold;

        if(white){

            if(++white_count[*index] >= CAL_SAMPLES)limit = 1;
        }else{

            white_count[*index] = 0;
        }
    }else if(!reference && (recalibration_mask & (1U << sensor->channel))){

        if(!adc_read(sensor->channel, &value_adc))return 0;

        sensor_calibration_update(sensor, value_adc);
    }

    if(++*index >= SENSORS_QUANTITY)*index = 0;

    return limit;

}

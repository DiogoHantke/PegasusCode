#include "calibration_store.h"
#include "pegasus_config.h"

#define RECORD_SIZE 27U
#define RECORD_DATA 8U
#define RECORD_CRC 24U

static uint16_t record_crc(const uint8_t *record){

    uint16_t crc = 0xFFFFU;

    for (uint8_t i = 0; i < RECORD_CRC; i++){

        crc ^= (uint16_t)record[i] << 8;

        for (uint8_t bit = 0; bit < 8; bit++){

            if (crc & 0x8000U){

                crc = (crc << 1) ^ 0x1021U;

            }else{

                crc <<= 1;

            }
        }
    }

    return crc;
}

uint8_t calibration_store_read(SENSOR *frontal, SENSOR *lateral){

    uint8_t record[RECORD_SIZE];

    if (flash_calibration_validate() != FLASH_OK || flash_calibration_read(record, sizeof record) != FLASH_OK){

        return 0;
    }

    if (record[0] != 'P' || record[1] != 'G' || record[2] != 'C' || record[3] != 'L' || record[4] != 1 || record[5] != RECORD_SIZE || record[6] != CAL_WHITE_HIGH || record[7] != SENSORS_QUANTITY || record[26] != 0xA5){

        return 0;
    }

    uint16_t crc = record[RECORD_CRC] | ((uint16_t)record[RECORD_CRC+1] << 8);

    if (crc != record_crc(record)){

        return 0;
    }

    for (uint8_t channel = 0; channel < SENSORS_QUANTITY; channel++){

        uint8_t min = record[RECORD_DATA + channel*2];
        uint8_t max = record[RECORD_DATA + channel*2 + 1];

        if (max < min || max - min < CAL_MIN_CONTRAST){

            return 0;
        }
    }

    for (uint8_t channel = 0; channel < SENSORS_QUANTITY; channel++){

        SENSOR *sensor = channel == 0 ? &lateral[0] : channel == 7 ? &lateral[1] : &frontal[channel-1];
        uint8_t min = record[RECORD_DATA + channel*2];
        uint8_t max = record[RECORD_DATA + channel*2 + 1];

        *sensor = (SENSOR){.channel = channel, .min = min, .max = max, .contrast = max-min, .aproved = 1};

    }

    return 1;
}

flash_status_t calibration_store_write(const SENSOR *frontal, const SENSOR *lateral){

    uint8_t record[RECORD_SIZE] = {'P', 'G', 'C', 'L', 1, RECORD_SIZE, CAL_WHITE_HIGH, SENSORS_QUANTITY};

    for (uint8_t channel = 0; channel < SENSORS_QUANTITY; channel++){

        const SENSOR *sensor = channel == 0 ? &lateral[0] : channel == 7 ? &lateral[1] : &frontal[channel-1];

        if (!sensor->aproved || sensor->max < sensor->min || sensor->max - sensor->min < CAL_MIN_CONTRAST){

            return FLASH_INVALID_ARGUMENT;
        }

        record[RECORD_DATA + channel*2] = sensor->min;
        record[RECORD_DATA + channel*2 + 1] = sensor->max;

    }

    uint16_t crc = record_crc(record);

    record[RECORD_CRC] = (uint8_t)crc;
    record[RECORD_CRC+1] = (uint8_t)(crc >> 8);
    record[26] = 0xA5;

    return flash_calibration_write(record, sizeof record);
}

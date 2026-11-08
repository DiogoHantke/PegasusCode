#include <avr/io.h>
#include "pegasus_config.h"
#include "calibration.h"
#include "calibration_store.h"
#include "adc_lib.h"
#include "pwm_lib.h"
#include "timer_lib.h"

static SENSOR frontal_sensors[SENSORS_QUANTITY-2] = {
    {.channel = QTR01_F},
    {.channel = QTR02_F},
    {.channel = QTR03_F},
    {.channel = QTR04_F},
    {.channel = QTR05_F},
    {.channel = QTR06_F}
};

static SENSOR lateral_sensors[2] = {
    {.channel = QTR00_L},
    {.channel = QTR07_L}
};

static CALIBRATION_STATUS status;
static STATE_CALIBRATION next_phase;
static uint16_t phase_start, led_start, movement_start, reference_start, extra_start;
static uint8_t initialized, index, reverse, moving, reposition_step;
static uint8_t reference_index, reference_any, reference_candidate, reference_initialized;
static uint8_t initial_lateral_done;
static uint16_t reference_last_sample, reposition_start, last_update, button_since, button_pressed_since;
static uint8_t reference_scans, startup_started, button_raw, button_debounced, button_holding;


const SENSOR *calibration_sensor_get(uint8_t channel){

    if (channel == QTR00_L)return &lateral_sensors[0];

    if (channel == QTR07_L)return &lateral_sensors[1];

    if (channel >= QTR01_F && channel <= QTR06_F)return &frontal_sensors[channel-1];

    return 0;

}

CALIBRATION_STATUS calibration_status(void){

    return status;

}

static void motors_stop(void){

    pwm_set_duty(PWM_A, 0);
    pwm_set_duty(PWM_B, 0);
    moving = 0;

}

static void pending_reset(SENSOR *sensors, uint8_t count){

    for (uint8_t i = 0; i < count; i++){

        if (sensors[i].aproved)continue;

        uint8_t channel = sensors[i].channel;

        sensors[i] = (SENSOR){.channel = channel, .min = 255};

    }
}

static uint8_t group_valid(const SENSOR *sensors, uint8_t count){

    for (uint8_t i = 0; i < count; i++){

        if (!sensors[i].aproved || sensors[i].max < sensors[i].min || sensors[i].max - sensors[i].min < CAL_MIN_CONTRAST)return 0;

    }

    return 1;

}

static uint8_t group_references(const SENSOR *sensors, uint8_t count){

    uint8_t available = 0;

    for (uint8_t i = 0; i < count; i++){

        available |= group_valid(&sensors[i], 1);

    }
    return available;

}

static void failure_mask(void){

    status.failed_sensors = 0;

    for (uint8_t channel = 0; channel < SENSORS_QUANTITY; channel++){

        if (!group_valid(calibration_sensor_get(channel), 1))status.failed_sensors |= (1U << channel);

    }
}

static void fail(STATE *state, CALIBRATION_CAUSE cause){

    motors_stop();
    PORTB &= ~((1 << SENSOR_ON) | (1 << LED_L));
    timer1_stop();
    failure_mask();
    status.valid = 0;
    status.cause = cause;
    status.phase = CALIBRATION_FAULT;
    *state = EMERGENCY;

}

static void reference_reset(void){

    reference_index = 0;
    reference_any = 0;
    reference_initialized = 0;
    reference_candidate = 0;
    reference_scans = 0;
    reference_start = 0;
    reference_last_sample = 0;
    adc_init();

}

static void prepare_move(STATE_CALIBRATION phase, uint8_t backwards, uint16_t now){

    motors_stop();
    next_phase = phase;
    reverse = backwards;
    phase_start = now;
    status.phase = CALIBRATION_SETTLE;
    index = 0;
    reference_reset();

}

static void led_update(uint16_t now){

    uint16_t elapsed = (uint16_t)(now - led_start);

    if (elapsed >= CAL_LED_CYCLE_MS){

        led_start += (elapsed / CAL_LED_CYCLE_MS) * CAL_LED_CYCLE_MS;
        elapsed %= CAL_LED_CYCLE_MS;

    }

    if (elapsed < 500U || (elapsed >= 1000U && elapsed < 1500U))PORTB |= (1 << LED_L);

    else PORTB &= ~(1 << LED_L);

}

static void motor_ramp(uint16_t now){

    uint16_t elapsed = (uint16_t)(now - movement_start);
    uint8_t duty = PERCENT_MOTOR_CALIBRATE;

    if (elapsed < CAL_MOTOR_RAMP_MS)duty = (uint32_t)elapsed * PERCENT_MOTOR_CALIBRATE / CAL_MOTOR_RAMP_MS;

    if (!MOTORS_KEY)duty = 0;

    pwm_set_duty(PWM_A, duty);
    pwm_set_duty(PWM_B, duty);

}

static void acquire_group(SENSOR *sensors, uint8_t count){

    uint8_t value;
    SENSOR *sensor = &sensors[index];

    if (!sensor->aproved){

        if (!adc_read(sensor->channel, &value))return;

        sensor_calibration_update(sensor, value);

        if (sensor->min <= sensor->max){

            sensor->contrast = sensor->max - sensor->min;

            if (sensor->contrast >= CAL_MIN_CONTRAST)sensor->aproved = 1;

        }
    }

    if (++index >= count)index = 0;

}

static int8_t reference_read(SENSOR *sensors, uint8_t count, uint16_t now){

    uint8_t value;

    if (!group_references(sensors, count)){

        reference_reset();
        return -2;
    }

    if (reference_scans && (uint16_t)(now - reference_last_sample) > CAL_REFERENCE_MAX_GAP_MS){

        reference_reset();
    }

    SENSOR *sensor = &sensors[reference_index];

    if (group_valid(sensor, 1)){

        if (!adc_read(sensor->channel, &value)){

            return -1;
        }

        uint16_t normalized = sensor_normalize(value, sensor->min, sensor->max);

        reference_any |= CAL_WHITE_HIGH ? normalized >= CAL_REFERENCE_LEVEL : normalized <= CAL_REFERENCE_LEVEL;

    }

    if (++reference_index < count){

        return -1;
    }

    reference_index = 0;

    uint8_t present = reference_any;

    reference_any = 0;
    reference_last_sample = now;

    if (!reference_initialized || present != reference_candidate){

        reference_candidate = present;
        reference_start = now;
        reference_initialized = 1;
        reference_scans = 1;
        return -1;
    }

    if (reference_scans < CAL_REFERENCE_MIN_SCANS){

        ++reference_scans;
    }

    if (reference_scans < CAL_REFERENCE_MIN_SCANS || (uint16_t)(now - reference_start) < CAL_REFERENCE_CONFIRM_MS){

        return -1;
    }

    return present;
}

uint8_t calibration_is_valid(void){

    return status.valid && status.phase == CALIBRATION_READY && group_valid(frontal_sensors, SENSORS_QUANTITY-2) && group_valid(lateral_sensors, 2);
}

void calibration_reset(void){

    motors_stop();
    PORTB &= ~((1 << SENSOR_ON) | (1 << LED_L));
    timer1_stop();

    status = (CALIBRATION_STATUS){.phase = INIT_CALIBRATION, .failed_sensors = 0xFF, .persistence = FLASH_NOT_CONFIGURED};
    next_phase = INIT_CALIBRATION;
    phase_start = led_start = movement_start = extra_start = 0;
    reposition_start = last_update = button_since = button_pressed_since = 0;
    initialized = initial_lateral_done = index = reverse = reposition_step = 0;
    startup_started = button_raw = button_debounced = button_holding = 0;
    reference_reset();

    for (uint8_t i = 0; i < SENSORS_QUANTITY-2; i++){

        frontal_sensors[i].aproved = 0;

    }

    for (uint8_t i = 0; i < 2; i++){

        lateral_sensors[i].aproved = 0;

    }

    pending_reset(frontal_sensors, SENSORS_QUANTITY-2);
    pending_reset(lateral_sensors, 2);

}

void calibration_abort(STATE *state){

    fail(state, CAL_FAULT_INTERRUPTED);

}

static void ready(STATE *state, uint8_t restored){

    motors_stop();
    PORTB &= ~(1 << SENSOR_ON);
    timer1_stop();
    status.valid = 1;
    status.restored = restored;
    status.failed_sensors = 0;
    status.cause = CAL_FAULT_NONE;
    status.phase = CALIBRATION_READY;

    if (!restored){

        status.persistence = calibration_store_write(frontal_sensors, lateral_sensors);

    }else{

        status.persistence = FLASH_OK;

    }

    PORTB |= (1 << LED_L);
    *state = WAITING;

}

void calibration_init_update(STATE *state){

    if (*state != INIT){

        return;
    }

    motors_stop();

    if (!startup_started){

        timer1_start();
        button_since = timer1_elapsed_milliseconds();
        button_raw = CAL_BUTTON_ACTIVE_LOW ? !(PIND & (1 << BUTTON)) : !!(PIND & (1 << BUTTON));
        startup_started = 1;

    }

    uint16_t now = timer1_elapsed_milliseconds();
    uint8_t pressed = CAL_BUTTON_ACTIVE_LOW ? !(PIND & (1 << BUTTON)) : !!(PIND & (1 << BUTTON));

    if (pressed != button_raw){

        button_raw = pressed;
        button_since = now;
        button_debounced = 0;
        button_holding = 0;

    }

    if ((uint16_t)(now - button_since) < CAL_BUTTON_DEBOUNCE_MS){

        return;
    }

    if (!button_debounced){

        button_debounced = 1;
        button_pressed_since = now;
        button_holding = pressed;

    }

    if (pressed && button_holding && (uint16_t)(now - button_pressed_since) < BUTTON_TIMEOUT_SECONDS * 1000U){

        return;
    }

    if (pressed && button_holding && calibration_store_read(frontal_sensors, lateral_sensors)){

        ready(state, 1);
        return;
    }

    *state = CALIBRATION;

}

void calibration_sensor(STATE *state) {

    if (*state != CALIBRATION){

        if (moving || status.phase == CALIBRATION_SETTLE){

            fail(state, CAL_FAULT_INTERRUPTED);

        }

        return;
    }

    if (!initialized){

        timer1_start();
        phase_start = led_start = last_update = timer1_elapsed_milliseconds();
        initialized = 1;
        motors_stop();
        pending_reset(frontal_sensors, SENSORS_QUANTITY-2);
        pending_reset(lateral_sensors, 2);

    }

    uint16_t now = timer1_elapsed_milliseconds();

    if ((uint16_t)(now - last_update) > CAL_UPDATE_MAX_GAP_MS){

        fail(state, CAL_FAULT_TIMING);
        return;
    }

    last_update = now;
    led_update(now);

    uint8_t repositioning = status.phase == FRONT_REPOSITION || status.phase == LATERAL_REPOSITION || (status.phase == CALIBRATION_SETTLE && (next_phase == FRONT_REPOSITION || next_phase == LATERAL_REPOSITION));

    if (repositioning && (uint16_t)(now - reposition_start) >= CAL_REPOSITION_TOTAL_MS){

        fail(state, CAL_FAULT_MOVEMENT);
        return;
    }

    if (moving){

        uint16_t limit = CAL_TIMEOUT_SECONDS * 1000U;

        if (status.phase == FRONT_POSITION){

            limit = CAL_POSITION_TIMEOUT_MS;

        }else if (repositioning){

            limit = CAL_REPOSITION_TIMEOUT_MS;

        }

        if ((uint16_t)(now - phase_start) >= limit){

            motors_stop();

            if (status.phase == FRONT_CALIBRATION){

                if (!initial_lateral_done){

                    status.phase = FRONT_POSITION;
                    phase_start = now;
                    reference_reset();

                }else{

                    status.phase = DIAGNOSTIC;

                }

            }else if (status.phase == LATERAL_CALIBRATION){

                initial_lateral_done = 1;
                status.phase = DIAGNOSTIC;

            }else{

                fail(state, CAL_FAULT_MOVEMENT);

            }

            return;
        }

        motor_ramp(now);

    }

    switch (status.phase){

    case INIT_CALIBRATION:

        PORTB |= (1 << SENSOR_ON);
        prepare_move(FRONT_CALIBRATION, 0, now);
        break;

    case CALIBRATION_SETTLE:

        if ((uint16_t)(now - phase_start) < CAL_MOTOR_SETTLE_MS)break;

        if (reverse){

            PORTB &= ~(1 << MOTOR_L_IN1);
            PORTB |= (1 << MOTOR_L_IN2);
            PORTD &= ~(1 << MOTOR_R_IN1);
            PORTD |= (1 << MOTOR_R_IN2);

        }else{

            PORTB &= ~(1 << MOTOR_L_IN2);
            PORTB |= (1 << MOTOR_L_IN1);
            PORTD &= ~(1 << MOTOR_R_IN2);
            PORTD |= (1 << MOTOR_R_IN1);

        }

        status.phase = next_phase;
        phase_start = movement_start = now;
        moving = 1;
        break;

    case FRONT_CALIBRATION:

        acquire_group(frontal_sensors, SENSORS_QUANTITY-2);

        if (!group_valid(frontal_sensors, SENSORS_QUANTITY-2)){

            break;
        }

        motors_stop();

        if (!initial_lateral_done){

            status.phase = FRONT_POSITION;
            phase_start = now;
            reference_reset();

        }else{

            status.phase = DIAGNOSTIC;

        }
        break;

    case FRONT_POSITION: {

        if (!group_references(frontal_sensors, SENSORS_QUANTITY-2)){

            fail(state, CAL_FAULT_REFERENCE);
            break;
        }

        if ((uint16_t)(now - phase_start) >= CAL_POSITION_TIMEOUT_MS){

            fail(state, CAL_FAULT_MOVEMENT);
            break;
        }

        int8_t present = reference_read(frontal_sensors, SENSORS_QUANTITY-2, now);

        if (present == 1){

            prepare_move(LATERAL_CALIBRATION, 1, now);

        }else if (present == 0 && !moving){

            prepare_move(FRONT_POSITION, 0, now);

        }

        break;
    }

    case LATERAL_CALIBRATION:

        acquire_group(lateral_sensors, 2);

        if (!group_valid(lateral_sensors, 2)){

            break;
        }

        motors_stop();
        initial_lateral_done = 1;
        status.phase = DIAGNOSTIC;
        break;

    case DIAGNOSTIC:

        motors_stop();
        failure_mask();

        if (!status.failed_sensors){

            ready(state, 0);

        }else if (!group_valid(frontal_sensors, SENSORS_QUANTITY-2)){

            if (status.frontal_retries >= NUMBER_ATTEMPTS){

                fail(state, CAL_FAULT_ATTEMPTS);
                break;
            }

            if (!group_references(frontal_sensors, SENSORS_QUANTITY-2)){

                fail(state, CAL_FAULT_REFERENCE);
                break;
            }

            ++status.frontal_retries;
            reposition_start = now;
            reposition_step = 0;
            prepare_move(FRONT_REPOSITION, 1, now);

        }else{

            if (status.lateral_retries >= NUMBER_ATTEMPTS){

                fail(state, CAL_FAULT_ATTEMPTS);
                break;
            }

            if (!group_references(lateral_sensors, 2)){

                fail(state, CAL_FAULT_REFERENCE);
                break;
            }

            ++status.lateral_retries;
            reposition_start = now;
            reposition_step = 0;
            prepare_move(LATERAL_REPOSITION, 0, now);

        }

        break;

    case FRONT_REPOSITION: {

        if ((uint16_t)(now - phase_start) >= CAL_REPOSITION_TIMEOUT_MS){

            fail(state, CAL_FAULT_MOVEMENT);
            break;
        }

        if (reposition_step == 2){

            if ((uint16_t)(now - extra_start) >= CAL_REPOSITION_EXTRA_MS){

                pending_reset(frontal_sensors, SENSORS_QUANTITY-2);
                prepare_move(FRONT_CALIBRATION, 0, now);

            }

            break;
        }

        int8_t present = reference_read(frontal_sensors, SENSORS_QUANTITY-2, now);

        if (present == -2){

            fail(state, CAL_FAULT_REFERENCE);
            break;
        }

        if (present == 1){

            reposition_step = 1;

        }

        if (present == 0 && reposition_step == 1){

            reposition_step = 2;
            extra_start = now;

        }

        break;
    }

    case LATERAL_REPOSITION: {

        if ((uint16_t)(now - phase_start) >= CAL_REPOSITION_TIMEOUT_MS){

            fail(state, CAL_FAULT_MOVEMENT);
            break;
        }

        if (reposition_step == 0){

            if (reference_read(frontal_sensors, SENSORS_QUANTITY-2, now) == 1){

                reposition_step = 1;
                prepare_move(LATERAL_REPOSITION, 1, now);

            }

        }else if (group_references(lateral_sensors, 2)){

            if (reference_read(lateral_sensors, 2, now) == 1){

                pending_reset(lateral_sensors, 2);
                prepare_move(LATERAL_CALIBRATION, 1, now);

            }

        }else{

            fail(state, CAL_FAULT_REFERENCE);

        }

        break;
    }

    case CALIBRATION_READY:

        if (!calibration_is_valid()){

            fail(state, CAL_FAULT_REFERENCE);
            break;
        }

        motors_stop();
        PORTB |= (1 << LED_L);
        *state = WAITING;
        break;

    case CALIBRATION_FAULT:

    default:

        fail(state, status.cause ? status.cause : CAL_FAULT_MOVEMENT);
        break;
    }
}

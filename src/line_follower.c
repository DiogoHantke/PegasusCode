/* Logica de Main (1).ino em C, com os limites e protecoes da corrida. */
#include <avr/io.h>
#include <stdint.h>

#include "line_follower.h"
#include "pegasus_config.h"
#include "calibration.h"
#include "adc_lib.h"
#include "pwm_lib.h"
#include "timer_lib.h"

static const uint8_t sensor_order[SENSORS_QUANTITY] = {
    QTR06_F, QTR05_F, QTR04_F, QTR03_F, QTR02_F, QTR01_F, QTR07_L, QTR00_L
};

static uint8_t initialized;
static uint8_t index_sensor;
static uint8_t samples[SENSORS_QUANTITY];
static uint8_t valid_mask;
static uint16_t side_value, stop_value;
static uint8_t loss_pending, curve, side_last, marker_candidate, marker_pending;
static uint8_t finish_armed, finish_inhibited;
static int16_t last_error, last_position;
static uint16_t last_frame, frame_started, loss_started;
static uint16_t marker_changed, finish_changed, finish_inhibited_since;

static int16_t constrain_value(int32_t value, int16_t limit){

    if(value > limit)return limit;
    if(value < -limit)return -limit;

    return (int16_t)value;

}

static void stop_motors(void){

    pwm_set_raw(PWM_B, 0);
    pwm_set_raw(PWM_A, 0);

}

static void motor_left(int16_t value){

    value = constrain_value(value, MOTOR_PWM_MAX);

    if(value >= 0){

        PORTB |= (1 << MOTOR_L_FORWARD);
        PORTB &= ~(1 << MOTOR_L_REVERSE);

    }else{

        PORTB &= ~(1 << MOTOR_L_FORWARD);
        PORTB |= (1 << MOTOR_L_REVERSE);
        value = -value;

    }

    pwm_set_raw(PWM_B, value);

}

static void motor_right(int16_t value){

    value = constrain_value(value, MOTOR_PWM_MAX);

    if(value >= 0){

        PORTD |= (1 << MOTOR_R_FORWARD);
        PORTD &= ~(1 << MOTOR_R_REVERSE);

    }else{

        PORTD &= ~(1 << MOTOR_R_FORWARD);
        PORTD |= (1 << MOTOR_R_REVERSE);
        value = -value;

    }

    pwm_set_raw(PWM_A, value);

}

void line_follower_reset(void){

    if(initialized){

        stop_motors();
        PORTB &= ~(1 << SENSOR_ON);

    }

    initialized = 0;

}

static void line_fault(STATE *state){

    line_follower_reset();
    PORTB &= ~(1 << SENSOR_ON);
    *state = EMERGENCY;

}

static uint8_t normalize_sensor(uint8_t channel, uint16_t raw, uint8_t *value){

    uint8_t low, high, approved;
    calibration_get(channel, &low, &high, &approved);

    if(!approved || high <= low || high - low < CAL_MIN_CONTRAST)return 0;

    /* A calibracao guarda oito bits; os extremos retornam para a escala 0..1023. */
    uint16_t minimum = (uint16_t)low * 4U;
    uint16_t maximum = (uint16_t)high * 4U;

    if(raw < minimum)raw = minimum;
    if(raw > maximum)raw = maximum;

    uint8_t scaled = (uint32_t)(raw - minimum) * 100U / (maximum - minimum);
    *value = CAL_WHITE_HIGH ? scaled : 100U - scaled;

    return 1;

}

static int16_t get_position(uint16_t sum, int16_t weighted, uint8_t online){

    int16_t position;

    if(online){

        /* O sketch primeiro trunca prom = -2.5*s[0] ... +2.5*s[5]. */
        int16_t prom = weighted / 2;
        position = (int32_t)prom * 100 / sum;

    }else{

        position = last_position < 0 ? -255 : 255;

    }

    last_position = position;
    return position;

}

static int16_t pid_line(int16_t position, float kp, float kd, int16_t limit){

    int16_t error = position;
    int16_t derivative = error - last_error;
    last_error = error;

    int16_t correction = error * kp + derivative * kd;

    return constrain_value(correction, limit);

}

static void curve_line(uint16_t now, uint8_t crossing){

    if((valid_mask & 0x81U) != 0x81U)return;

    /* A0 marca curva enquanto A7 esta fora da marca de parada. */
    uint8_t side = !crossing && side_value <= LINE_MARKER_RAW_THRESHOLD &&
                   stop_value > LINE_FINISH_RAW_THRESHOLD;

    if(side != marker_candidate){

        marker_candidate = side;
        marker_changed = now;

    }

    if((uint16_t)(now - marker_changed) >= LINE_MARKER_DEBOUNCE_MS){

        if(marker_candidate && !side_last)curve = !curve;
        side_last = marker_candidate;

    }

}

static uint8_t finish_line(uint16_t now){

    if(stop_value >= LINE_FINISH_RELEASE_RAW_THRESHOLD){

        finish_armed = 1;
        marker_pending = 0;

    }else if(stop_value > LINE_FINISH_RAW_THRESHOLD){

        marker_pending = 0;

    }else if(finish_inhibited){

        finish_armed = marker_pending = 0;

    }else if(finish_armed){

        if(!marker_pending){

            marker_pending = 1;
            finish_changed = now;

        }

        if((uint16_t)(now - finish_changed) >= LINE_FINISH_DEBOUNCE_MS)return 1;

    }else{

        marker_pending = 0;

    }

    return 0;

}

void line_follower_step(STATE *state){

    if(!state || *state != RUNNING)return;

    uint16_t now = timer1_elapsed_milliseconds();

    if(!initialized){

        stop_motors();
        initialized = 1;
        index_sensor = curve = side_last = marker_candidate = marker_pending = 0;
        finish_armed = finish_inhibited = loss_pending = 0;
        valid_mask = 0;
        last_error = last_position = 0;
        frame_started = marker_changed = now;
        last_frame = now - LINE_PERIOD_MS;
        PORTB |= (1 << SENSOR_ON);

    }

    if((uint16_t)(now - frame_started) >= LINE_ADC_TIMEOUT_MS ||
       (loss_pending && (uint16_t)(now - loss_started) >= LINE_LOST_TIMEOUT_MS)){

        line_fault(state);
        return;

    }

    uint8_t channel = sensor_order[index_sensor];
    uint16_t raw;

    if(!adc_read_10(channel, &raw))return;

    if(normalize_sensor(channel, raw, &samples[channel]))valid_mask |= 1U << channel;
    else{
        valid_mask &= ~(1U << channel);
        samples[channel] = 0;
    }

    if(channel == QTR00_L)side_value = raw;
    if(channel == QTR07_L)stop_value = raw;
    if(++index_sensor < SENSORS_QUANTITY)return;

    index_sensor = 0;
    frame_started = now;

    uint16_t sum = 0;
    int16_t weighted = 0;
    uint8_t active = 0, available = 0;

    for(uint8_t index = 0; index < 6; index++){

        if(!(valid_mask & (1U << (QTR06_F - index))))continue;
        available++;
        uint8_t sensor = samples[QTR06_F - index];
        if(sensor < LINE_ACTIVE_THRESHOLD)continue;
        active++;
        sum += sensor;
        weighted += (int16_t)(2 * (int16_t)index - 5) * sensor;

    }

    if(!available){
        line_fault(state);
        return;
    }
    uint8_t online = active != 0;
    uint8_t crossing = active >= 4U;
    if(crossing){
        finish_inhibited = 1;
        finish_inhibited_since = now;
        marker_pending = 0;
    }else if(finish_inhibited &&
             (uint16_t)(now - finish_inhibited_since) >= LINE_FINISH_CROSSING_HOLD_MS){
        finish_inhibited = 0;
    }
    if(finish_line(now)){
        line_follower_reset();
        *state = FINISH;
        return;
    }

    int16_t position = get_position(sum, weighted, online);

    if(!online && !loss_pending){

        loss_pending = 1;
        loss_started = now;

    }

    if(loss_pending){

        stop_motors();
        marker_candidate = side_last = 0;
        marker_changed = now;

        if(!online)return;

        loss_pending = 0;
        last_error = position;

    }

    if((uint16_t)(now - last_frame) < LINE_PERIOD_MS)return;

    last_frame = now;

    int16_t base = curve ? LINE_CURVE_SPEED : LINE_BASE_SPEED;
    int16_t limit = curve ? LINE_CURVE_TURN_LIMIT : LINE_TURN_LIMIT;
    int16_t correction = pid_line(position, LINE_KP, LINE_KD, limit);
    /* Erro negativo aponta para A6 e reduz a velocidade da roda esquerda. */
    int16_t left = base + correction;
    int16_t right = base - correction;

    /* Como no loop do sketch, a marca altera a velocidade do proximo passo. */
    curve_line(now, crossing);
    motor_left(left);
    motor_right(right);

}

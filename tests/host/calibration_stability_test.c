#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <avr/io.h>
#include "pegasus_config.h"
#include "calibration.h"
#include "calibration_store.h"
#include "adc_lib.h"
#include "pwm_lib.h"
#include "timer_lib.h"

uint8_t DDRD, DDRB, PORTD, PORTB, PIND;
uint8_t ADMUX, ADCSRA, ADCH;
uint8_t TCCR0A, TCCR0B, TCNT0, OCR0A, OCR0B;
static uint16_t clock_now;
static uint8_t timer_running;
static unsigned reads, validates, writes;
static uint8_t stored[27], stored_available;
static unsigned conversion_delay;

void timer1_start(void){ timer_running = 1; }
void timer1_stop(void){ timer_running = 0; }
uint16_t timer1_elapsed_milliseconds(void){ return clock_now; }

#ifndef TEST_REAL_FLASH
flash_status_t flash_calibration_validate(void){
    validates++;
    return stored_available ? FLASH_OK : FLASH_NOT_CONFIGURED;
}
flash_status_t flash_calibration_read(void *data, uint16_t size){
    reads++;
    assert(size == sizeof stored);
    memcpy(data, stored, size);
    return stored_available ? FLASH_OK : FLASH_NOT_CONFIGURED;
}
flash_status_t flash_calibration_write(const void *data, uint16_t size){
    writes++;
    assert(!OCR0A && !OCR0B && !timer_running);
    assert(size == sizeof stored);
    memcpy(stored, data, size);
    return FLASH_NOT_CONFIGURED;
}
#endif

static uint16_t independent_crc(const uint8_t *bytes, unsigned size){
    uint16_t crc = 65535;
    while (size--){
        uint8_t value = *bytes++;
        for (uint8_t bit = 0x80; bit; bit >>= 1){
            uint8_t feedback = !!(crc & 0x8000) ^ !!(value & bit);
            crc <<= 1;
            if (feedback)crc ^= 0x1021;
        }
    }
    return crc;
}

static void fixture_crc(void){
    uint16_t crc = independent_crc(stored, 24);
    stored[24] = crc;
    stored[25] = crc >> 8;
}

static void valid_record(void){
    const uint8_t header[] = {'P','G','C','L',1,27,CAL_WHITE_HIGH,8};
    memcpy(stored, header, sizeof header);
    for (unsigned channel = 0; channel < 8; channel++){
        stored[8+2*channel] = 30;
        stored[9+2*channel] = 220;
    }
    fixture_crc();
    stored[26] = 0xA5;
    stored_available = 1;
}

static void button(uint8_t pressed){
    PIND = (CAL_BUTTON_ACTIVE_LOW ? !pressed : pressed) ? (1 << BUTTON) : 0;
}

static void reset(uint16_t origin){
    adc_init();
    pwm_init();
    calibration_reset();
    clock_now = origin;
    button(0);
    reads = validates = writes = 0;
    conversion_delay = 0;
    CALIBRATION_STATUS info = calibration_status();
    assert(info.phase == INIT_CALIBRATION && !info.valid && !info.restored);
    assert(!info.frontal_retries && !info.lateral_retries && info.cause == CAL_FAULT_NONE);
    assert(info.failed_sensors == 255 && !timer_running && !OCR0A && !OCR0B);
    assert(!(PORTB & ((1 << LED_L) | (1 << SENSOR_ON))));
    for (unsigned i = 0; i < 8; i++){
        const SENSOR *sensor = calibration_sensor_get(i);
        assert(sensor && sensor->channel == i && sensor->min == 255 && sensor->max == 0);
        assert(!sensor->aproved && !sensor->count_mean && !sensor->value_mean && !sensor->transition_pending);
    }
    assert(!calibration_sensor_get(8) && !calibration_is_valid());
}

static void startup_test(const char *mode){
    reset(65000);
    STATE state = INIT;
    uint8_t short_press = !strcmp(mode, "short_button");
    uint8_t bounce = !strcmp(mode, "bounce_button");
    valid_record();
    if (!strcmp(mode, "missing_record"))stored_available = 0;
    if (!strcmp(mode, "invalid_record"))stored[10] ^= 1;
    for (unsigned tick = 0; tick < 6000 && state == INIT; tick++){
        uint8_t pressed = 1;
        if (short_press && tick >= 700)pressed = 0;
        if (bounce && tick == 1000)pressed = 0;
        button(pressed);
        calibration_init_update(&state);
        assert(!OCR0A && !OCR0B);
        if (tick < 2030)assert(!validates && state != WAITING);
        if (bounce && tick < 3031)assert(!validates);
        if (timer_running)clock_now++;
    }
    if (short_press){
        assert(state == CALIBRATION && !validates && !calibration_is_valid());
    }else if (!strcmp(mode, "missing_record") || !strcmp(mode, "invalid_record")){
        assert(state == CALIBRATION && validates == 1 && !calibration_is_valid());
        for (unsigned i = 0; i < 8; i++)assert(!calibration_sensor_get(i)->aproved);
    }else{
        assert(state == WAITING && calibration_is_valid());
        assert(calibration_status().restored && (PORTB & (1 << LED_L)) && !timer_running);
        assert(validates == 1 && reads == 1 && writes == 0);
        calibration_init_update(&state);
        assert(validates == 1);
    }
    puts(mode);
}

static void corrupt_records(void){
    SENSOR frontal[6], lateral[2], old_frontal[6], old_lateral[2];
    memset(frontal, 0x55, sizeof frontal);
    memset(lateral, 0x66, sizeof lateral);
    memcpy(old_frontal, frontal, sizeof frontal);
    memcpy(old_lateral, lateral, sizeof lateral);
    assert(independent_crc((const uint8_t *)"123456789", 9) == 0x29B1);
    for (unsigned i = 0; i < sizeof stored; i++){
        valid_record();
        stored[i] ^= 1;
        assert(!calibration_store_read(frontal, lateral));
        assert(!memcmp(frontal, old_frontal, sizeof frontal));
        assert(!memcmp(lateral, old_lateral, sizeof lateral));
    }
    for (unsigned i = 0; i < 8; i++){
        valid_record();
        stored[8+2*i] = 220;
        stored[9+2*i] = 30;
        fixture_crc();
        assert(!calibration_store_read(frontal, lateral));
        valid_record();
        stored[9+2*i] = 40;
        fixture_crc();
        assert(!calibration_store_read(frontal, lateral));
    }
    valid_record();
    assert(calibration_store_read(frontal, lateral));
    for (unsigned i = 0; i < 6; i++)assert(frontal[i].aproved && frontal[i].channel == i+1);
    puts("CRC, metadata, limites e carga transacional");
}

static void filter_test(void){
    SENSOR sensor = {.min = 255};
    uint8_t means[] = {30,30,220,30,30,220,220};
    for (unsigned i = 0; i < sizeof means; i++){
        for (unsigned n = 0; n < 10; n++)sensor_calibration_update(&sensor, means[i]);
        assert(!sensor.count_mean && !sensor.value_mean);
        if (i >= 1 && i <= 5)assert(sensor.min == 30 && sensor.max == 30);
    }
    assert(sensor.min == 30 && sensor.max == 220 && !sensor.transition_pending);
    SENSOR edge = {.min = 255};
    for (unsigned i = 0; i < 10; i++)sensor_calibration_update(&edge, 20);
    for (unsigned i = 0; i < 10; i++)sensor_calibration_update(&edge, 70);
    assert(edge.transition_pending);
    for (unsigned i = 0; i < 10; i++)sensor_calibration_update(&edge, 80);
    assert(edge.transition_pending);
    for (unsigned i = 0; i < 10; i++)sensor_calibration_update(&edge, 89);
    assert(!edge.transition_pending && edge.min == 80 && edge.max == 80);
    puts("Filtro: dez amostras, anomalia transitória e limiares exatos");
}

static STATE scenario(const char *mode, uint16_t origin){
    reset(origin);
    stored_available = 0;
    STATE state = INIT;
    unsigned samples[8] = {0}, phase_tick = 0, zero_since = 0;
    SENSOR frozen[8];
    uint8_t frozen_valid[8] = {0};
    STATE_CALIBRATION last_phase = INIT_CALIBRATION;
    uint8_t direction = PORTB & ((1 << MOTOR_L_IN1) | (1 << MOTOR_L_IN2));
    uint8_t front_passes = 0, lateral_passes = 0;
    unsigned cal_tick = 0, position_entry = 0;
    uint8_t in_cal = 0;
    uint8_t white = CAL_WHITE_HIGH ? 220 : 30;
    uint8_t dark = CAL_WHITE_HIGH ? 30 : 220;
    unsigned tick;

    for (tick = 0; tick < 120000 && state != WAITING && state != EMERGENCY; tick++){
        CALIBRATION_STATUS before = calibration_status();
        if (before.phase != last_phase){
            last_phase = before.phase;
            phase_tick = tick;
            if (last_phase == FRONT_CALIBRATION){ front_passes++; memset(samples, 0, sizeof samples); }
            if (last_phase == LATERAL_CALIBRATION){ lateral_passes++; memset(samples, 0, sizeof samples); }
            if (last_phase == FRONT_POSITION)position_entry++;
        }
        uint8_t raw = dark;
        if (ADCSRA & (1 << ADSC)){
            if (strcmp(mode, "adc_stuck") && ++conversion_delay >= 2){
                uint8_t channel = ADMUX & 15;
                assert(channel < 8);
                if (before.phase == FRONT_CALIBRATION || before.phase == LATERAL_CALIBRATION){
                    if (before.phase == FRONT_CALIBRATION)assert(channel >= 1 && channel <= 6);
                    else assert(channel == 0 || channel == 7);
                    raw = samples[channel]++ < 20 ? 30 : 220;
                    uint8_t front_bad = channel == 3 || (!strcmp(mode, "multiple") && channel == 4);
                    if ((!strcmp(mode, "front_retry") || !strcmp(mode, "front_exhaust") || !strcmp(mode, "front_missing") || !strcmp(mode, "oscillating") || !strcmp(mode, "multiple")) && front_bad && (front_passes == 1 || !strcmp(mode, "front_exhaust")))raw = 30;
                    if ((!strcmp(mode, "lateral_retry") || !strcmp(mode, "lateral_exhaust") || !strcmp(mode, "lateral_missing") || !strcmp(mode, "front_search_missing") || !strcmp(mode, "multiple")) && channel == 0 && (lateral_passes == 1 || !strcmp(mode, "lateral_exhaust")))raw = 30;
                    if (!strcmp(mode, "other_lateral") && channel == 7 && lateral_passes == 1)raw = 30;
                    if (!strcmp(mode, "both_lateral") && (channel == 0 || channel == 7))raw = 30;
                    if (!strcmp(mode, "no_reference"))raw = 30;
                }else if (before.phase == FRONT_POSITION){
                    raw = tick-phase_tick >= 150 ? white : dark;
                    if (!strcmp(mode, "position_missing"))raw = dark;
                }else if (before.phase == FRONT_REPOSITION){
                    raw = tick-phase_tick < 300 ? white : dark;
                    if (!strcmp(mode, "front_missing"))raw = dark;
                    if (!strcmp(mode, "oscillating"))raw = (tick/10)%2 ? white : dark;
                }else if (before.phase == LATERAL_REPOSITION){
                    raw = white;
                    if (!strcmp(mode, "lateral_missing") && (channel == 0 || channel == 7))raw = dark;
                    if (!strcmp(mode, "front_search_missing") && channel >= 1 && channel <= 6)raw = dark;
                }
                ADCH = raw;
                ADCSRA &= ~(1 << ADSC);
                conversion_delay = 0;
            }
        }else conversion_delay = 0;

        if (state == INIT){
            calibration_init_update(&state);
        }else{
            if (!in_cal){ in_cal = 1; cal_tick = tick; }
            calibration_sensor(&state);
        }
        CALIBRATION_STATUS after = calibration_status();
        uint8_t new_direction = PORTB & ((1 << MOTOR_L_IN1) | (1 << MOTOR_L_IN2));
        if (new_direction != direction){
            assert(!OCR0A && !OCR0B);
            assert(tick-zero_since >= CAL_MOTOR_SETTLE_MS);
            direction = new_direction;
        }
        if (OCR0A || OCR0B)zero_since = tick+1;
        if (!MOTORS_KEY)assert(!OCR0A && !OCR0B);
        if (in_cal && state == CALIBRATION){
            unsigned led_phase = (tick-cal_tick)%CAL_LED_CYCLE_MS;
            assert(!!(PORTB & (1 << LED_L)) == (led_phase < 500 || (led_phase >= 1000 && led_phase < 1500)));
        }
        if (after.phase == CALIBRATION_SETTLE || after.phase == DIAGNOSTIC)assert(!OCR0A && !OCR0B);
        for (unsigned channel = 0; channel < 8; channel++){
            const SENSOR *sensor = calibration_sensor_get(channel);
            if (frozen_valid[channel])assert(!memcmp(sensor, &frozen[channel], sizeof *sensor));
            else if (sensor->aproved){ frozen[channel] = *sensor; frozen_valid[channel] = 1; }
        }
        if (timer_running)clock_now++;
    }
    assert(tick < 120000);
    CALIBRATION_STATUS result = calibration_status();
    assert(!OCR0A && !OCR0B && !timer_running);
    assert(!(PORTB & (1 << SENSOR_ON)));
    if (!strcmp(mode, "success") || !strcmp(mode, "front_retry") || !strcmp(mode, "lateral_retry") || !strcmp(mode, "other_lateral") || !strcmp(mode, "multiple")){
        assert(state == WAITING && result.valid && calibration_is_valid());
        assert(PORTB & (1 << LED_L));
        assert(result.cause == CAL_FAULT_NONE && !result.failed_sensors && position_entry >= 1);
#ifndef TEST_REAL_FLASH
        assert(writes == 1);
        assert(independent_crc(stored, 24) == (uint16_t)(stored[24] | (stored[25] << 8)));
#endif
        assert(result.persistence == FLASH_NOT_CONFIGURED);
        if (!strcmp(mode, "front_retry"))assert(result.frontal_retries == 1 && result.lateral_retries == 0);
        if (!strcmp(mode, "lateral_retry") || !strcmp(mode, "other_lateral"))assert(result.frontal_retries == 0 && result.lateral_retries == 1);
        if (!strcmp(mode, "multiple"))assert(result.frontal_retries == 1 && result.lateral_retries == 1);
    }else{
        assert(state == EMERGENCY && !result.valid && !calibration_is_valid());
        assert(!(PORTB & (1 << LED_L)) && !writes);
        if (!strcmp(mode, "front_exhaust"))assert(result.cause == CAL_FAULT_ATTEMPTS && result.frontal_retries == 3 && front_passes == 4);
        else if (!strcmp(mode, "lateral_exhaust"))assert(result.cause == CAL_FAULT_ATTEMPTS && result.lateral_retries == 3 && lateral_passes == 4);
        else if (!strcmp(mode, "no_reference") || !strcmp(mode, "both_lateral") || !strcmp(mode, "adc_stuck"))assert(result.cause == CAL_FAULT_REFERENCE);
        else assert(result.cause == CAL_FAULT_MOVEMENT);
        for (unsigned i = 0; i < 10; i++){
            calibration_sensor(&state);
            calibration_init_update(&state);
            assert(state == EMERGENCY && !OCR0A && !OCR0B);
        }
    }
    printf("PASS %s, origin=%u, F=%u L=%u\n", mode, origin, front_passes, lateral_passes);
    return state;
}

static void interruption_test(void){
    reset(0);
    STATE state = CALIBRATION;
    for (unsigned tick = 0; tick < 600; tick++){
        calibration_sensor(&state);
        clock_now++;
    }
    if (MOTORS_KEY)assert(OCR0A && OCR0B);
    state = STOP;
    calibration_sensor(&state);
    assert(state == EMERGENCY && !OCR0A && !OCR0B && !calibration_is_valid());
    reset(0);
    state = CALIBRATION;
    for (unsigned tick = 0; tick < 600; tick++){
        calibration_sensor(&state);
        clock_now++;
    }
    clock_now += CAL_UPDATE_MAX_GAP_MS+1;
    calibration_sensor(&state);
    assert(state == EMERGENCY && calibration_status().cause == CAL_FAULT_TIMING);
    assert(!OCR0A && !OCR0B && !(PORTB & (1 << LED_L)));
    puts("Interrupção externa e atraso de supervisão param imediatamente");
}

int main(void){
    const char *modes[] = {"success","front_retry","lateral_retry","other_lateral","multiple","front_exhaust","lateral_exhaust","no_reference","both_lateral","front_missing","lateral_missing","front_search_missing","oscillating","position_missing","adc_stuck"};
    for (unsigned i = 0; i < sizeof modes/sizeof modes[0]; i++)scenario(modes[i], 0);
    scenario("success", 65000);
    scenario("multiple", 65500);
    scenario("front_exhaust", 65000);
    /* Consecutive executions in this same process retain C static storage. */
    scenario("success", clock_now);
    scenario("success", clock_now);
    scenario("no_reference", clock_now);
    scenario("success", clock_now);
    interruption_test();
#ifndef TEST_REAL_FLASH
    startup_test("short_button");
    startup_test("long_button");
    startup_test("bounce_button");
    startup_test("missing_record");
    startup_test("invalid_record");
    corrupt_records();
#else
    (void)startup_test;
    (void)corrupt_records;
#endif
    filter_test();
    puts("PASS calibration stability suite");
}

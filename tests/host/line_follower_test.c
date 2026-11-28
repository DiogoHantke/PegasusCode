#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pegasus_config.h"
#include "line_follower.h"
#include "pwm_lib.h"

uint8_t DDRD, DDRB, PORTD, PORTB, PIND;
static uint16_t now;
static uint16_t values[8];
static uint8_t left, right, pending, adc_stalled, calibration_bad, failed_mask;
static STATE state;

uint16_t timer1_elapsed_milliseconds(void){ return now; }
void pwm_set_raw(pwm_channel_t channel, uint8_t value){
    assert(value <= MOTOR_PWM_MAX);
    if(channel == PWM_B)left = value;
    else right = value;
}
void pwm_set_duty(pwm_channel_t channel, uint8_t percent){
    assert(percent == 0);
    pwm_set_raw(channel, 0);
}
void calibration_get(uint8_t channel, uint8_t *low, uint8_t *high, uint8_t *ok){
    *low = 20;
    *high = calibration_bad ? 20 : 220;
    *ok = !calibration_bad && !(failed_mask & (1U << channel));
}
uint8_t adc_read_10(uint8_t channel, uint16_t *value){
    if(adc_stalled)return 0;
    pending = !pending;
    if(pending)return 0;
    *value = values[channel];
    return 1;
}
static int16_t left_command(void){
    return PORTB & (1 << MOTOR_L_IN2) ? left : -(int16_t)left;
}
static int16_t right_command(void){
    return PORTD & (1 << MOTOR_R_IN1) ? right : -(int16_t)right;
}
static void white(uint8_t channel, uint8_t percent){
    if(!CAL_WHITE_HIGH)percent = 100 - percent;
    values[channel] = (20U + percent * 2U) * 4U;
}
/* Uma varredura completa por ms, com conversoes assincronas. */
static void tick(void){
    now++;
    for(int i = 0; i < 16; i++)line_follower_step(&state);
}
static void wait_ms(unsigned duration){
    while(duration--)tick();
}
static void center(void){
    for(uint8_t c = 0; c < 8; c++)white(c, 0);
    white(3, 100);
    white(4, 100);
    values[QTR00_L] = 1023;
    values[QTR07_L] = 1023;
}
static void begin(uint16_t time){
    line_follower_reset();
    now = time;
    pending = adc_stalled = calibration_bad = failed_mask = 0;
    state = RUNNING;
    center();
}
static void setup(uint16_t time){
    begin(time);
    wait_ms(30);
    assert(state == RUNNING && left == LINE_BASE_SPEED && right == LINE_BASE_SPEED);
}
int main(void){
    /* Nao interfere com os motores da calibracao fora da corrida. */
    left = right = 20;
    line_follower_reset();
    assert(left == 20 && right == 20);
    begin(0);
    for(unsigned i = 0; i < 14; i++)line_follower_step(&state);
    assert(left == 0 && right == 0); /* Ainda falta A0 no primeiro quadro. */
    line_follower_step(&state);
    line_follower_step(&state);
    assert(left == LINE_BASE_SPEED && right == LINE_BASE_SPEED); /* Primeiro quadro, sem janela de media. */
    assert((PORTB & (1 << MOTOR_L_IN2)) && !(PORTB & (1 << MOTOR_L_IN1)));
    assert((PORTD & (1 << MOTOR_R_IN1)) && !(PORTD & (1 << MOTOR_R_IN2)));
    assert(PORTB & (1 << SENSOR_ON));

    /* Erro +10: PD do sketch (1.2/4.3), seguido da parcela proporcional. */
    setup(0);
    white(3, 90);
    white(4, 60);
    tick(); /* A alteracao entra no proximo passo, sem filtro de amostras. */
    assert(left_command() == LINE_BASE_SPEED && right_command() == LINE_BASE_SPEED - 55);
    wait_ms(5);
    assert(left_command() == LINE_BASE_SPEED && right_command() == LINE_BASE_SPEED - 12);
    setup(0);

    /* Um unico quadro com desvio aparece na correcao, sem atenuacao temporal. */
    white(6, 100);
    tick();
    assert(right_command() > left_command());
    white(6, 0);
    wait_ms(10);
    assert(left == LINE_BASE_SPEED && right == LINE_BASE_SPEED);
    for(uint8_t c = 1; c <= 6; c++)white(c, c >= 5 ? 100 : 0);
    wait_ms(40);
    assert(right_command() > left_command());
    setup(0);
    for(uint8_t c = 1; c <= 6; c++)white(c, c <= 2 ? 100 : 0);
    wait_ms(40);
    assert(left_command() > right_command());

    /* Perda na leitura direta para no primeiro quadro; recuperacao breve nao trava. */
    setup(0);
    for(uint8_t c = 1; c <= 6; c++)white(c, 0);
    tick();
    assert(state == RUNNING && left == 0 && right == 0);
    wait_ms(35);
    assert(left == 0 && right == 0);
    center();
    wait_ms(40);
    assert(state == RUNNING && left == LINE_BASE_SPEED && right == LINE_BASE_SPEED);

    /* A0 nao substitui a linha frontal nem marca chegada. */
    for(uint8_t c = 1; c <= 6; c++)white(c, 0);
    values[QTR00_L] = 30;
    tick();
    wait_ms(LINE_LOST_TIMEOUT_MS - 1);
    assert(state == RUNNING && left == 0 && right == 0);
    tick();
    assert(state == EMERGENCY && left == 0 && right == 0);
    assert(!(PORTB & (1 << SENSOR_ON)));
    center();
    wait_ms(300);
    assert(state == EMERGENCY && left == 0 && right == 0);

    /* Uma faixa branca estreita com fundo levemente refletivo ainda orienta para A6. */
    begin(0);
    for(uint8_t c = 1; c <= 6; c++)white(c, LINE_ACTIVE_THRESHOLD - 2U);
    white(6, 100);
    wait_ms(LINE_LOST_TIMEOUT_MS + 1U);
    assert(state == RUNNING && right_command() > left_command());

    /* Sem linha na partida nunca movimenta. */
    begin(0);
    for(uint8_t c = 1; c <= 6; c++)white(c, 0);
    wait_ms(LINE_LOST_TIMEOUT_MS + 1);
    assert(state == EMERGENCY && left == 0 && right == 0);

    /* Uma marca em A0 alterna entre reta e curva sem encerrar a corrida. */
    setup(0);
    values[QTR00_L] = 30;
    values[QTR07_L] = LINE_FINISH_RELEASE_RAW_THRESHOLD;
    wait_ms(LINE_MARKER_DEBOUNCE_MS - 1);
    assert(left == LINE_BASE_SPEED && right == LINE_BASE_SPEED);
    wait_ms(60);
    assert(left == LINE_CURVE_SPEED && right == LINE_CURVE_SPEED);
    wait_ms(200);
    assert(left == LINE_CURVE_SPEED);
    values[QTR00_L] = 31;
    wait_ms(180);
    values[QTR00_L] = 30;
    wait_ms(180);
    assert(left == LINE_BASE_SPEED);

    /* Cruzamento nao alterna velocidade nem finaliza. */
    setup(0);
    values[QTR00_L] = 30;
    values[QTR07_L] = LINE_FINISH_RAW_THRESHOLD;
    for(uint8_t c = 1; c <= 6; c++)white(c, 100);
    wait_ms(9500);
    assert(state == RUNNING && left == LINE_BASE_SPEED && right == LINE_BASE_SPEED);

    /* Quatro frontais bloqueiam A7, inclusive na virada do timer. */
    setup(64500U);
    white(2, 100);
    white(5, 100);
    values[QTR07_L] = LINE_FINISH_RAW_THRESHOLD;
    wait_ms(1000);
    assert(state == RUNNING);
    center();
    values[QTR07_L] = LINE_FINISH_RAW_THRESHOLD;
    wait_ms(LINE_FINISH_CROSSING_HOLD_MS + LINE_FINISH_DEBOUNCE_MS);
    assert(state == RUNNING && left == LINE_BASE_SPEED && right == LINE_BASE_SPEED);
    values[QTR07_L] = LINE_FINISH_RELEASE_RAW_THRESHOLD - 1U;
    wait_ms(10);
    values[QTR07_L] = LINE_FINISH_RAW_THRESHOLD;
    wait_ms(LINE_FINISH_DEBOUNCE_MS + 1U);
    assert(state == RUNNING);
    values[QTR07_L] = LINE_FINISH_RELEASE_RAW_THRESHOLD;
    wait_ms(10);
    values[QTR07_L] = LINE_FINISH_RAW_THRESHOLD;
    wait_ms(LINE_FINISH_DEBOUNCE_MS + 1U);
    assert(state == FINISH && left == 0 && right == 0);

    /* Fundo escuro em toda a frente nao bloqueia a marca de A7. */
    setup(0);
    for(uint8_t c = 1; c <= 6; c++)white(c, 0);
    values[QTR07_L] = LINE_FINISH_RAW_THRESHOLD;
    wait_ms(LINE_FINISH_DEBOUNCE_MS + 1U);
    assert(state == FINISH && left == 0 && right == 0);

    /* A7 identifica a marca pelo valor bruto mesmo sem calibracao lateral. */
    begin(0);
    failed_mask = 1U << QTR07_L;
    wait_ms(30);
    values[QTR07_L] = LINE_FINISH_RAW_THRESHOLD;
    wait_ms(LINE_FINISH_DEBOUNCE_MS - 1U);
    assert(state == RUNNING);
    wait_ms(2);
    assert(state == FINISH && left == 0 && right == 0);
    assert(!(PORTB & (1 << SENSOR_ON)));

    /* Sem saida da marca inicial em A7, nao finaliza. */
    begin(0);
    values[QTR07_L] = LINE_FINISH_RAW_THRESHOLD;
    wait_ms(9500);
    assert(state == RUNNING);

    /* Reinicio e perda de linha atravessando a virada de uint16_t. */
    setup(65460U);
    for(uint8_t c = 1; c <= 6; c++)white(c, 0);
    wait_ms(LINE_LOST_TIMEOUT_MS + 1);
    assert(state == EMERGENCY && left == 0 && right == 0);
    setup(65520U);
    adc_stalled = 1;
    wait_ms(LINE_ADC_TIMEOUT_MS);
    assert(state == EMERGENCY && left == 0 && right == 0);
    assert(!(PORTB & (1 << SENSOR_ON)));
    setup(0);
    calibration_bad = 1;
    tick();
    assert(state == EMERGENCY && left == 0 && right == 0);

    /* Laterais oscilantes sao lidos diretamente; nao existe watchdog de filtro. */
    setup(0);
    for(unsigned i = 0; i < 400; i++){
        values[QTR07_L] = i % 2 ? 0 : 1023;
        tick();
    }
    assert(state == RUNNING && left == LINE_BASE_SPEED && right == LINE_BASE_SPEED);
    /* Desabilitar corrida interrompe PWM e reinicia a aquisicao na proxima partida. */
    setup(0);
    state = STOP;
    line_follower_reset();
    assert(left == 0 && right == 0);
    setup(0);
    /* Falhas laterais e parciais nao impedem corrida nem criam falsas marcas. */
    setup(0);
    failed_mask = 0x81U;
    values[0] = 0;
    values[7] = 1023;
    wait_ms(10000);
    assert(state == RUNNING && left == LINE_BASE_SPEED && right == LINE_BASE_SPEED);
    setup(0);
    failed_mask = 0x7EU & ~(1U << 3);
    wait_ms(1000);
    assert(state == RUNNING && (left || right));
    puts("line follower: OK (including partial sensor failure)");
}

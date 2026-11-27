#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pegasus_config.h"
#include "line_follower.h"
#include "pwm_lib.h"

uint8_t DDRD, DDRB, PORTD, PORTB, PIND;
static uint16_t now;
static uint16_t values[8];
static uint8_t left, right, pending, adc_stalled, calibration_bad;
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
    (void)channel;
    *low = 20;
    *high = calibration_bad ? 20 : 220;
    *ok = !calibration_bad;
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
static void black(uint8_t channel, uint8_t percent){
    if(CAL_WHITE_HIGH)percent = 100 - percent;
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
    for(uint8_t c = 0; c < 8; c++)black(c, 0);
    black(3, 100);
    black(4, 100);
    values[QTR00_L] = 1023;
    values[QTR07_L] = 0;
}
static void begin(uint16_t time){
    line_follower_reset();
    now = time;
    pending = adc_stalled = calibration_bad = 0;
    state = RUNNING;
    center();
}
static void setup(uint16_t time){
    begin(time);
    wait_ms(30);
    assert(state == RUNNING && left == 80 && right == 80);
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
    assert(left == 80 && right == 80); /* Primeiro quadro, sem janela de media. */
    assert((PORTB & (1 << MOTOR_L_IN2)) && !(PORTB & (1 << MOTOR_L_IN1)));
    assert((PORTD & (1 << MOTOR_R_IN1)) && !(PORTD & (1 << MOTOR_R_IN2)));
    assert(PORTB & (1 << SENSOR_ON));

    /* Erro +10: PD do sketch (1.2/4.3), seguido da parcela proporcional. */
    setup(0);
    black(3, 90);
    black(4, 60);
    tick(); /* A alteracao entra no proximo passo, sem filtro de amostras. */
    assert(left_command() == 25 && right_command() == 80);
    wait_ms(5);
    assert(left_command() == 68 && right_command() == 80);
    setup(0);

    /* Um unico quadro com desvio aparece na correcao, sem atenuacao temporal. */
    black(6, 100);
    tick();
    assert(left_command() > right_command());
    black(6, 0);
    wait_ms(10);
    assert(left == 80 && right == 80);
    for(uint8_t c = 1; c <= 6; c++)black(c, c >= 5 ? 100 : 0);
    wait_ms(40);
    assert(left_command() > right_command());
    setup(0);
    for(uint8_t c = 1; c <= 6; c++)black(c, c <= 2 ? 100 : 0);
    wait_ms(40);
    assert(right_command() > left_command());

    /* Perda na leitura direta para no primeiro quadro; recuperacao breve nao trava. */
    setup(0);
    for(uint8_t c = 1; c <= 6; c++)black(c, 0);
    tick();
    assert(state == RUNNING && left == 0 && right == 0);
    wait_ms(35);
    assert(left == 0 && right == 0);
    center();
    wait_ms(40);
    assert(state == RUNNING && left == 80 && right == 80);

    /* Laterais pretos nao escondem perda frontal; emergencia nao rearma sozinha. */
    for(uint8_t c = 1; c <= 6; c++)black(c, 0);
    values[QTR07_L] = 30;
    tick();
    wait_ms(LINE_LOST_TIMEOUT_MS - 1);
    assert(state == RUNNING && left == 0 && right == 0);
    tick();
    assert(state == EMERGENCY && left == 0 && right == 0);
    assert(!(PORTB & (1 << SENSOR_ON)));
    center();
    wait_ms(300);
    assert(state == EMERGENCY && left == 0 && right == 0);

    /* Sem linha na partida nunca movimenta. */
    begin(0);
    for(uint8_t c = 1; c <= 6; c++)black(c, 0);
    wait_ms(LINE_LOST_TIMEOUT_MS + 1);
    assert(state == EMERGENCY && left == 0 && right == 0);

    /* Flanco lateral direto e estavel alterna 80/46 uma unica vez. */
    setup(0);
    values[QTR00_L] = 30;
    values[QTR07_L] = 30;
    wait_ms(LINE_MARKER_DEBOUNCE_MS - 1);
    assert(left == 80 && right == 80);
    wait_ms(60);
    assert(left == 46 && right == 46);
    wait_ms(200);
    assert(left == 46);
    values[QTR07_L] = 29;
    wait_ms(180);
    values[QTR07_L] = 30;
    wait_ms(180);
    assert(left == 80);

    /* Cruzamento nao alterna velocidade nem finaliza. */
    setup(0);
    values[QTR00_L] = 30;
    values[QTR07_L] = 30;
    for(uint8_t c = 1; c <= 6; c++)black(c, 100);
    wait_ms(9500);
    assert(state == RUNNING && left == 80 && right == 80);

    /* Chegada apos 9s precisa confirmar branco por 120ms. */
    setup(0);
    values[QTR00_L] = 30;
    wait_ms(200);
    assert(state == RUNNING);
    values[QTR00_L] = 31;
    wait_ms(9000);
    assert(state == RUNNING);
    values[QTR00_L] = 30;
    wait_ms(LINE_MARKER_DEBOUNCE_MS - 1);
    assert(state == RUNNING);
    wait_ms(60);
    assert(state == FINISH && left == 0 && right == 0);
    assert(!(PORTB & (1 << SENSOR_ON)));

    /* Sem saida da marca inicial, nao finaliza. */
    begin(0);
    values[QTR00_L] = 30;
    wait_ms(9500);
    assert(state == RUNNING);

    /* Reinicio e perda de linha atravessando a virada de uint16_t. */
    setup(65460U);
    for(uint8_t c = 1; c <= 6; c++)black(c, 0);
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
    assert(state == RUNNING && left == 80 && right == 80);
    /* Desabilitar corrida interrompe PWM e reinicia a aquisicao na proxima partida. */
    setup(0);
    state = STOP;
    line_follower_reset();
    assert(left == 0 && right == 0);
    setup(0);
    puts("line follower: OK");
}

#include <assert.h>
#include <stdio.h>
#include "button.h"
#include "pegasus_config.h"

static uint16_t now;
static uint8_t step(unsigned duration, uint8_t down){
    uint8_t events = 0;
    while(duration--)events |= button_update(++now, down);
    return events;
}
int main(void){
    button_init(now, 0);
    assert(!step(10, 1));
    assert(!step(40, 0)); /* Ruido nao vira toque. */
    assert(step(60, 1) == BUTTON_PRESS);
    assert(step(40, 0) == BUTTON_SHORT);
    assert(step(1000, 1) == BUTTON_PRESS);
    assert(!step(40, 0));
    assert(step(1500, 1) == BUTTON_PRESS);
    assert(!step(40, 0));
    assert(step(2001, 1) == (BUTTON_PRESS | BUTTON_LONG));
    assert(!step(5000, 1));
    assert(!step(40, 0));
    now = 65500;
    button_init(now, 0);
    assert(step(60, 1) == BUTTON_PRESS);
    assert(step(40, 0) == BUTTON_SHORT);
    button_init(now, 1);
    assert(!step(3000, 1)); /* Boot com botao preso exige soltar primeiro. */
    assert(!step(40, 0));
    assert(step(60, 1) == BUTTON_PRESS);
    assert(step(40, 0) == BUTTON_SHORT);
    puts("button: debounce, short/long boundaries, boot-held and wrap OK");
}

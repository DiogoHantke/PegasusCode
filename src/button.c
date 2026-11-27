#include "button.h"
#include "pegasus_config.h"

static uint8_t last, stable, armed, active, handled;
static uint16_t changed, started;

void button_init(uint16_t now, uint8_t raw){

    last = stable = !!raw;
    armed = !raw;
    active = handled = 0;
    changed = started = now;

}

uint8_t button_update(uint16_t now, uint8_t raw){

    uint8_t events = 0;
    raw = !!raw;
    if(raw != last){
        last = raw;
        changed = now;
    }
    if(raw != stable && (uint16_t)(now - changed) >= BUTTON_DEBOUNCE_MS){
        stable = raw;
        if(stable && armed){
            active = 1;
            handled = 0;
            started = changed;
            events |= BUTTON_PRESS;
        }else if(!stable){
            if(active && !handled){
                uint16_t duration = (uint16_t)(changed - started);
                if(duration >= BUTTON_LONG_MS)events |= BUTTON_LONG;
                else if(duration < BUTTON_SHORT_MS)events |= BUTTON_SHORT;
            }
            active = 0;
            armed = 1;
        }
    }
    if(active && !handled && stable && raw &&
       (uint16_t)(now - started) >= BUTTON_LONG_MS){
        handled = 1;
        events |= BUTTON_LONG;
    }
    return events;

}
